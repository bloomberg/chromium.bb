// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/crw_cert_verification_controller.h"

#include <memory>

#include "base/ios/block_types.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/scoped_block.h"
#import "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/worker_pool.h"
#include "ios/web/net/cert_verifier_block_adapter.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/certificate_policy_cache.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#include "net/cert/cert_verify_result.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

// Enum for Web.CertVerifyAgreement UMA metric to report certificate
// verification mismatch between SecTrust API and CertVerifier. SecTrust API is
// used for making load/no-load decision and CertVerifier is used for getting
// the reason of verification failure. It is expected that mismatches will
// happen for those 2 approaches (e.g. SecTrust API accepts the cert but
// CertVerifier rejects it). This metric helps to understand how common
// mismatches are.
// Note: This enum is used to back an UMA histogram, and should be treated as
// append-only.
enum CertVerifyAgreement {
  // There is no mismach. Both SecTrust API and CertVerifier accepted this cert.
  CERT_VERIFY_AGREEMENT_ACCEPTED_BY_BOTH = 0,
  // There is no mismach. Both SecTrust API and CertVerifier rejected this cert.
  CERT_VERIFY_AGREEMENT_REJECTED_BY_BOTH,
  // SecTrust API accepted the cert, but CertVerifier rejected it (e.g. MDM cert
  // or CertVerifier was more strict during verification), this mismach is
  // expected to be common because of MDM certs.
  CERT_VERIFY_AGREEMENT_ACCEPTED_ONLY_BY_IOS,
  // SecTrust API rejected the cert, but CertVerifier accepted it. This mismatch
  // is expected to be uncommon.
  CERT_VERIFY_AGREEMENT_ACCEPTED_ONLY_BY_NSS,
  CERT_VERIFY_AGREEMENT_COUNT,
};

// This class takes ownership of block and releases it on UI thread, even if
// |BlockHolder| is destructed on a background thread. On destruction calls its
// block with default arguments, if block was not called before. This way
// BlockHolder guarantees that block is always called to satisfy API contract
// for CRWCertVerificationController (completion handlers are always called).
template <class T>
class BlockHolder : public base::RefCountedThreadSafe<BlockHolder<T>> {
 public:
  // Takes ownership of |block|, which must not be null. On destruction calls
  // this block with |DefaultArgs|, if block was not called before.
  // |DefaultArgs| must be passed by value, not by ponter or reference.
  template <typename... Arguments>
  BlockHolder(T block, Arguments... DefaultArgs)
      : block_([block copy]),
        called_(false),
        default_block_([^{
          block(DefaultArgs...);
        } copy]) {
    DCHECK(block_);
  }

  // Calls underlying block with the given variadic arguments.
  template <typename... Arguments>
  void call(Arguments... Args) {
    block_(Args...);
    called_ = true;
  }

 private:
  BlockHolder() = delete;
  friend class base::RefCountedThreadSafe<BlockHolder>;

  // Finalizes object's destruction on UI thread by calling |default_block| (if
  // |block| has not been called yet) and releasing all blocks. Must be called
  // on UI thread.
  static void Finalize(id block,
                       ProceduralBlock default_block,
                       bool block_was_called) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    // By calling default_block, BlockHolder guarantees that block is always
    // called to satisfy API contract for CRWCertVerificationController
    // (completion handlers are always called).
    if (!block_was_called)
      default_block();
    [block release];
    [default_block release];
  }

  // Releases underlying |block_| on UI thread, calls |default_block_| if
  // |block_| has not been called yet.
  ~BlockHolder() {
    if (web::WebThread::CurrentlyOn(web::WebThread::UI)) {
      Finalize(block_, default_block_, called_);
    } else {
      web::WebThread::PostTask(
          web::WebThread::UI, FROM_HERE,
          base::Bind(&BlockHolder::Finalize, block_, default_block_, called_));
    }
  }

  T block_;
  // true if this |block_| has already been called.
  bool called_;
  // Called on destruction if |block_| was not called.
  ProceduralBlock default_block_;
};

typedef scoped_refptr<BlockHolder<web::PolicyDecisionHandler>>
    PolicyDecisionHandlerHolder;

}  // namespace

@interface CRWCertVerificationController () {
  // Cert verification object which wraps |net::CertVerifier|. Must be created,
  // used and destroyed on IO Thread.
  std::unique_ptr<web::CertVerifierBlockAdapter> _certVerifier;

  // URLRequestContextGetter for obtaining net layer objects.
  net::URLRequestContextGetter* _contextGetter;

  // Used to remember user exceptions to invalid certs.
  scoped_refptr<web::CertificatePolicyCache> _certPolicyCache;
}

// Cert verification flags. Must be used on IO Thread.
@property(nonatomic, readonly) int certVerifyFlags;

// Returns YES if CertVerifier should be run (even if SecTrust API considers
// cert as valid) and Web.CertVerifyAgreement UMA metric should be reported.
// The result of this method is random and nondeterministic.
- (BOOL)shouldReportCertVerifyAgreement;

// Reports Web.CertVerifyAgreement UMA metric.
- (void)reportUMAForCertVerifyAgreement:(CertVerifyAgreement)agreement;

// Creates _certVerifier object on IO thread.
- (void)createCertVerifier;

// Decides the policy for the given |trust| which was rejected by iOS and the
// given |host| and calls |handler| on completion.
- (void)
decideLoadPolicyForRejectedTrustResult:(SecTrustResultType)trustResult
                           serverTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
                                  host:(NSString*)host
                     completionHandler:(PolicyDecisionHandlerHolder)handler;

// Decides the policy for the given |trust| which was accepted by iOS and the
// given |host| and calls |handler| on completion.
- (void)
decideLoadPolicyForAcceptedTrustResult:(SecTrustResultType)trustResult
                           serverTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
                                  host:(NSString*)host
                     completionHandler:(PolicyDecisionHandlerHolder)handler;

// Verifies the given |cert| for the given |host| using |net::CertVerifier| and
// calls |completionHandler| on completion. This method can be called on any
// thread. |completionHandler| cannot be null and will be called asynchronously
// on IO thread or will never be called if IO task can't start or complete.
- (void)verifyCert:(const scoped_refptr<net::X509Certificate>&)cert
              forHost:(NSString*)host
    completionHandler:(void (^)(net::CertVerifyResult))completionHandler;

// Verifies the given |trust| using SecTrustRef API. |completionHandler| cannot
// be null and will be either called asynchronously on Worker thread or will
// never be called if the worker task can't start or complete.
- (void)verifyTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
    completionHandler:(void (^)(SecTrustResultType))completionHandler;

// Returns cert accept policy for the given SecTrust result. |trustResult| must
// not be for a valid cert. Must be called on IO thread.
- (web::CertAcceptPolicy)
    loadPolicyForRejectedTrustResult:(SecTrustResultType)trustResult
                  certVerifierResult:(net::CertVerifyResult)certVerifierResult
                         serverTrust:(SecTrustRef)trust
                                host:(NSString*)host;

@end

@implementation CRWCertVerificationController

#pragma mark - Superclass

- (void)dealloc {
  DCHECK(!_certVerifier);
  [super dealloc];
}

#pragma mark - Public

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState {
  DCHECK(browserState);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  self = [super init];
  if (self) {
    _contextGetter = browserState->GetRequestContext();
    DCHECK(_contextGetter);
    _certPolicyCache =
        web::BrowserState::GetCertificatePolicyCache(browserState);

    [self createCertVerifier];
  }
  return self;
}

- (void)decideLoadPolicyForTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
                            host:(NSString*)host
               completionHandler:(web::PolicyDecisionHandler)completionHandler {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  // completionHandler of |verifyCert:forHost:completionHandler:| is called on
  // IO thread and then bounces back to UI thread. As a result all objects
  // captured by completionHandler may be released on either UI or IO thread.
  // Since |completionHandler| can potentially capture multiple thread unsafe
  // objects (like Web Controller) |completionHandler| itself should never be
  // released on background thread and |BlockHolder| ensures that.
  __block PolicyDecisionHandlerHolder handlerHolder(
      new BlockHolder<web::PolicyDecisionHandler>(
          completionHandler, web::CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR,
          net::CertStatus()));
  [self verifyTrust:trust
      completionHandler:^(SecTrustResultType trustResult) {
        if (web::GetSecurityStyleFromTrustResult(trustResult) ==
            web::SECURITY_STYLE_AUTHENTICATED) {
          [self decideLoadPolicyForAcceptedTrustResult:trustResult
                                           serverTrust:trust
                                                  host:host
                                     completionHandler:handlerHolder];
        } else {
          [self decideLoadPolicyForRejectedTrustResult:trustResult
                                           serverTrust:trust
                                                  host:host
                                     completionHandler:handlerHolder];
        }
      }];
}

- (void)querySSLStatusForTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
                          host:(NSString*)host
             completionHandler:(web::StatusQueryHandler)completionHandler {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  // The completion handlers of |verifyCert:forHost:completionHandler:| and
  // |verifyTrust:completionHandler:| will be deallocated on background thread.
  // |completionHandler| itself should never be released on background thread
  // and |BlockHolder| ensures that.
  __block scoped_refptr<BlockHolder<web::StatusQueryHandler>> handlerHolder(
      new BlockHolder<web::StatusQueryHandler>(
          completionHandler, web::SECURITY_STYLE_UNKNOWN, net::CertStatus()));
  [self verifyTrust:trust
      completionHandler:^(SecTrustResultType trustResult) {
        web::SecurityStyle securityStyle =
            web::GetSecurityStyleFromTrustResult(trustResult);
        if (securityStyle == web::SECURITY_STYLE_AUTHENTICATED) {
          // SecTrust API considers this cert as valid.
          dispatch_async(dispatch_get_main_queue(), ^{
            handlerHolder->call(securityStyle, net::CertStatus());
          });
          return;
        }

        // Retrieve the net::CertStatus for invalid certificates to determine
        // the rejection reason, it is possible that rejection reason could not
        // be determined and |cert_status| will be empty.
        scoped_refptr<net::X509Certificate> cert(
            web::CreateCertFromTrust(trust));
        [self verifyCert:cert
                      forHost:host
            completionHandler:^(net::CertVerifyResult certVerifierResult) {
              dispatch_async(dispatch_get_main_queue(), ^{
                handlerHolder->call(securityStyle,
                                    certVerifierResult.cert_status);
              });
            }];
      }];
}

- (void)allowCert:(scoped_refptr<net::X509Certificate>)cert
          forHost:(NSString*)host
           status:(net::CertStatus)status {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  // Store user decisions with the leaf cert, ignoring any intermediates.
  // This is because WKWebView returns the verified certificate chain in
  // |webView:didReceiveAuthenticationChallenge:completionHandler:|,
  // but the server-supplied chain in
  // |webView:didFailProvisionalNavigation:withError:|.
  if (!cert->GetIntermediateCertificates().empty()) {
    cert = net::X509Certificate::CreateFromHandle(
        cert->os_cert_handle(), net::X509Certificate::OSCertHandles());
  }
  DCHECK(cert->GetIntermediateCertificates().empty());
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE, base::BindBlock(^{
    _certPolicyCache->AllowCertForHost(
        cert.get(), base::SysNSStringToUTF8(host), status);
  }));
}

- (void)shutDown {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE, base::BindBlock(^{
    // This block captures |self| delaying its deallocation and causing dealloc
    // to happen on either IO or UI thread (which is fine for this class).
    _certVerifier.reset();
  }));
}

#pragma mark - Private

- (int)certVerifyFlags {
  DCHECK(web::WebThread::CurrentlyOn(web::WebThread::IO));
  DCHECK(_contextGetter);
  // |net::URLRequestContextGetter| lifetime is expected to be at least the same
  // or longer than |BrowserState| lifetime.
  net::URLRequestContext* context = _contextGetter->GetURLRequestContext();
  DCHECK(context);
  net::SSLConfigService* SSLConfigService = context->ssl_config_service();
  DCHECK(SSLConfigService);
  net::SSLConfig config;
  SSLConfigService->GetSSLConfig(&config);
  return config.GetCertVerifyFlags();
}

- (BOOL)shouldReportCertVerifyAgreement {
  // Cert verification is an expensive operation. Since extra verification will
  // be used only for UMA reporting purposes, do that only for 1% of cases.
  return base::RandGenerator(100) == 0;
}

- (void)reportUMAForCertVerifyAgreement:(CertVerifyAgreement)agreement {
  UMA_HISTOGRAM_ENUMERATION("Web.CertVerifyAgreement", agreement,
                            CERT_VERIFY_AGREEMENT_COUNT);
}

- (void)createCertVerifier {
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE, base::BindBlock(^{
    net::URLRequestContext* context = _contextGetter->GetURLRequestContext();
    _certVerifier.reset(new web::CertVerifierBlockAdapter(
        context->cert_verifier(), context->net_log()));
  }));
}

- (void)
decideLoadPolicyForRejectedTrustResult:(SecTrustResultType)trustResult
                           serverTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
                                  host:(NSString*)host
                     completionHandler:(PolicyDecisionHandlerHolder)handler {
  // SecTrust API considers this cert as invalid. Check the reason and
  // whether or not user has decided to proceed with this bad cert.
  scoped_refptr<net::X509Certificate> cert(web::CreateCertFromTrust(trust));
  [self verifyCert:cert
                forHost:host
      completionHandler:^(net::CertVerifyResult certVerifierResult) {
        if (!net::IsCertStatusError(certVerifierResult.cert_status)) {
          // |cert_status| must not be no-error if SecTrust API considers the
          // cert as invalid. Otherwise there will be issues with errors
          // reporting and recovery.
          certVerifierResult.cert_status = net::CERT_STATUS_INVALID;
        }

        web::CertAcceptPolicy policy =
            [self loadPolicyForRejectedTrustResult:trustResult
                                certVerifierResult:certVerifierResult
                                       serverTrust:trust.get()
                                              host:host];

        dispatch_async(dispatch_get_main_queue(), ^{
          if ([self shouldReportCertVerifyAgreement]) {
            CertVerifyAgreement agreement =
                net::IsCertStatusError(certVerifierResult.cert_status)
                    ? CERT_VERIFY_AGREEMENT_REJECTED_BY_BOTH
                    : CERT_VERIFY_AGREEMENT_ACCEPTED_ONLY_BY_NSS;
            [self reportUMAForCertVerifyAgreement:agreement];
          }
          handler->call(policy, certVerifierResult.cert_status);
        });
      }];
}

- (void)
decideLoadPolicyForAcceptedTrustResult:(SecTrustResultType)trustResult
                           serverTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
                                  host:(NSString*)host
                     completionHandler:(PolicyDecisionHandlerHolder)handler {
  // SecTrust API considers this cert as valid.
  dispatch_async(dispatch_get_main_queue(), ^{
    handler->call(web::CERT_ACCEPT_POLICY_ALLOW, net::CertStatus());
  });

  if ([self shouldReportCertVerifyAgreement]) {
    // Execute CertVerifier::Verify to collect CertVerifyAgreement UMA.
    scoped_refptr<net::X509Certificate> cert(web::CreateCertFromTrust(trust));
    [self verifyCert:cert
                  forHost:host
        completionHandler:^(net::CertVerifyResult certVerifierResult) {
          // SecTrust API accepted this cert and |PolicyDecisionHandler| has
          // been called already. |CertVerifier| verification is executed only
          // to collect CertVerifyAgreement UMA.
          dispatch_async(dispatch_get_main_queue(), ^{
            CertVerifyAgreement agreement =
                net::IsCertStatusError(certVerifierResult.cert_status)
                    ? CERT_VERIFY_AGREEMENT_ACCEPTED_ONLY_BY_IOS
                    : CERT_VERIFY_AGREEMENT_ACCEPTED_BY_BOTH;
            [self reportUMAForCertVerifyAgreement:agreement];
          });
        }];
  }
}

- (void)verifyCert:(const scoped_refptr<net::X509Certificate>&)cert
              forHost:(NSString*)host
    completionHandler:(void (^)(net::CertVerifyResult))completionHandler {
  DCHECK(completionHandler);
  __block scoped_refptr<net::X509Certificate> blockCert = cert;
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE, base::BindBlock(^{
    // WeakNSObject does not work across different threads, hence this block
    // retains self.
    if (!_certVerifier) {
      completionHandler(net::CertVerifyResult());
      return;
    }

    web::CertVerifierBlockAdapter::Params params(std::move(blockCert),
                                                 base::SysNSStringToUTF8(host));
    params.flags = self.certVerifyFlags;
    // OCSP response is not provided by iOS API.
    // CRLSets are not used, as the OS is used to make load/no-load
    // decisions, not the CertVerifier.
    _certVerifier->Verify(params, ^(net::CertVerifyResult result, int) {
      completionHandler(result);
    });
  }));
}

- (void)verifyTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
    completionHandler:(void (^)(SecTrustResultType))completionHandler {
  DCHECK(completionHandler);
  // SecTrustEvaluate performs trust evaluation synchronously, possibly making
  // network requests. The UI thread should not be blocked by that operation.
  base::WorkerPool::PostTask(FROM_HERE, base::BindBlock(^{
    SecTrustResultType trustResult = kSecTrustResultInvalid;
    if (SecTrustEvaluate(trust.get(), &trustResult) != errSecSuccess) {
      trustResult = kSecTrustResultInvalid;
    }
    completionHandler(trustResult);
  }), false /* task_is_slow */);
}

- (web::CertAcceptPolicy)
    loadPolicyForRejectedTrustResult:(SecTrustResultType)trustResult
                  certVerifierResult:(net::CertVerifyResult)certVerifierResult
                         serverTrust:(SecTrustRef)trust
                                host:(NSString*)host {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK_NE(web::SECURITY_STYLE_AUTHENTICATED,
            web::GetSecurityStyleFromTrustResult(trustResult));

  if (trustResult != kSecTrustResultRecoverableTrustFailure ||
      SecTrustGetCertificateCount(trust) == 0) {
    // Trust result is not recoverable or leaf cert is missing.
    return web::CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;
  }

  // Check if user has decided to proceed with this bad cert.
  scoped_refptr<net::X509Certificate> leafCert =
      net::X509Certificate::CreateFromHandle(
          SecTrustGetCertificateAtIndex(trust, 0),
          net::X509Certificate::OSCertHandles());

  web::CertPolicy::Judgment judgment = _certPolicyCache->QueryPolicy(
      leafCert.get(), base::SysNSStringToUTF8(host),
      certVerifierResult.cert_status);

  return (judgment == web::CertPolicy::ALLOWED)
             ? web::CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_ACCEPTED_BY_USER
             : web::CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_UNDECIDED_BY_USER;
}

@end
