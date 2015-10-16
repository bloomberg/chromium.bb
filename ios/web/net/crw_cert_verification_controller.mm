// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/crw_cert_verification_controller.h"

#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#import "base/memory/ref_counted.h"
#import "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/worker_pool.h"
#include "ios/web/net/cert_verifier_block_adapter.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#include "net/cert/cert_verify_result.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

// This class takes ownership of block and releases it on UI thread, even if
// |BlockHolder| is destructed on a background thread.
template <class T>
class BlockHolder : public base::RefCountedThreadSafe<BlockHolder<T>> {
 public:
  // Takes ownership of |block|, which must not be null.
  explicit BlockHolder(T block) : block_([block copy]) { DCHECK(block_); }

  // Calls underlying block with the given variadic arguments.
  template <typename... Arguments>
  void call(Arguments... Args) {
    block_(Args...);
  }

 private:
  BlockHolder() = delete;
  friend class base::RefCountedThreadSafe<BlockHolder>;

  // Releases the given block, must be called on UI thread.
  static void ReleaseBlock(id block) {
    DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
    [block release];
  }

  // Releases underlying |block_| on UI thread.
  ~BlockHolder() {
    if (web::WebThread::CurrentlyOn(web::WebThread::UI)) {
      ReleaseBlock(block_);
    } else {
      web::WebThread::PostTask(web::WebThread::UI, FROM_HERE,
                               base::Bind(&BlockHolder::ReleaseBlock, block_));
    }
  }

  T block_;
};

}  // namespace

@interface CRWCertVerificationController () {
  // Cert verification object which wraps |net::CertVerifier|. Must be created,
  // used and destroyed on IO Thread.
  scoped_ptr<web::CertVerifierBlockAdapter> _certVerifier;

  // URLRequestContextGetter for obtaining net layer objects.
  net::URLRequestContextGetter* _contextGetter;
}

// Cert verification flags. Must be used on IO Thread.
@property(nonatomic, readonly) int certVerifyFlags;

// Creates _certVerifier object on IO thread.
- (void)createCertVerifier;

// Verifies the given |cert| for the given |host| using |net::CertVerifier| and
// calls |completionHandler| on completion. This method can be called on any
// thread. |completionHandler| cannot be null and will be called asynchronously
// on IO thread.
- (void)verifyCert:(const scoped_refptr<net::X509Certificate>&)cert
              forHost:(NSString*)host
    completionHandler:(void (^)(net::CertVerifyResult, int))completionHandler;

// Verifies the given |trust| using SecTrustRef API. |completionHandler| cannot
// be null and will be either called asynchronously on Worker thread or
// synchronously on current thread if the worker task can't start (in this
// case |dispatched| argument will be NO).
- (void)verifyTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
    completionHandler:(void (^)(SecTrustResultType, BOOL dispatched))handler;

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
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  self = [super init];
  if (self) {
    _contextGetter = browserState->GetRequestContext();
    DCHECK(_contextGetter);
    [self createCertVerifier];
  }
  return self;
}

- (void)decidePolicyForCert:(const scoped_refptr<net::X509Certificate>&)cert
                       host:(NSString*)host
          completionHandler:(web::PolicyDecisionHandler)completionHandler {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  // completionHandler of |verifyCert:forHost:completionHandler:| is called on
  // IO thread and then bounces back to UI thread. As a result all objects
  // captured by completionHandler may be released on either UI or IO thread.
  // Since |completionHandler| can potentially capture multiple thread unsafe
  // objects (like Web Controller) |completionHandler| itself should never be
  // released on background thread and |BlockHolder| ensures that.
  __block scoped_refptr<BlockHolder<web::PolicyDecisionHandler>> handlerHolder(
      new BlockHolder<web::PolicyDecisionHandler>(completionHandler));
  [self verifyCert:cert
                forHost:host
      completionHandler:^(net::CertVerifyResult result, int error) {
        web::CertAcceptPolicy policy =
            web::CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;
        if (error == net::OK) {
          policy = web::CERT_ACCEPT_POLICY_ALLOW;
        } else if (net::IsCertStatusError(result.cert_status)) {
          policy = net::IsCertStatusMinorError(result.cert_status)
                       ? web::CERT_ACCEPT_POLICY_ALLOW
                       : web::CERT_ACCEPT_POLICY_RECOVERABLE_ERROR;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
          handlerHolder->call(policy, result.cert_status);
        });
      }];
}

- (void)querySSLStatusForTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
                          host:(NSString*)host
             completionHandler:(web::StatusQueryHandler)completionHandler {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);

  // The completion handlers of |verifyCert:forHost:completionHandler:| and
  // |verifyTrust:completionHandler:| will be deallocated on background thread.
  // |completionHandler| itself should never be released on background thread
  // and |BlockHolder| ensures that.
  __block scoped_refptr<BlockHolder<web::StatusQueryHandler>> handlerHolder(
      new BlockHolder<web::StatusQueryHandler>(completionHandler));
  [self verifyTrust:trust
      completionHandler:^(SecTrustResultType trustResult, BOOL dispatched) {
        if (!dispatched) {
          // CertVerification task did not start.
          dispatch_async(dispatch_get_main_queue(), ^{
            handlerHolder->call(web::SECURITY_STYLE_UNKNOWN, net::CertStatus());
          });
          return;
        }

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
        // TODO(eugenebut): Add UMA for CertVerifier and SecTrust verification
        // mismatch (crbug.com/535699).
        scoped_refptr<net::X509Certificate> cert(
            web::CreateCertFromTrust(trust));
        [self verifyCert:cert
                      forHost:host
            completionHandler:^(net::CertVerifyResult certVerifierResult, int) {
              dispatch_async(dispatch_get_main_queue(), ^{
                handlerHolder->call(securityStyle,
                                    certVerifierResult.cert_status);
              });
            }];
      }];
}

- (void)shutDown {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
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

- (void)createCertVerifier {
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE, base::BindBlock(^{
    net::URLRequestContext* context = _contextGetter->GetURLRequestContext();
    _certVerifier.reset(new web::CertVerifierBlockAdapter(
        context->cert_verifier(), context->net_log()));
  }));
}

- (void)verifyCert:(const scoped_refptr<net::X509Certificate>&)cert
              forHost:(NSString*)host
    completionHandler:(void (^)(net::CertVerifyResult, int))completionHandler {
  DCHECK(completionHandler);
  __block scoped_refptr<net::X509Certificate> blockCert = cert;
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE, base::BindBlock(^{
        // WeakNSObject does not work across different threads, hence this block
        // retains self.
        if (!_certVerifier) {
          completionHandler(net::CertVerifyResult(), net::ERR_FAILED);
          return;
        }

        web::CertVerifierBlockAdapter::Params params(
            blockCert.Pass(), base::SysNSStringToUTF8(host));
        params.flags = self.certVerifyFlags;
        params.crl_set = net::SSLConfigService::GetCRLSet();
        // OCSP response is not provided by iOS API.
        _certVerifier->Verify(params, completionHandler);
      }));
}

- (void)verifyTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
    completionHandler:(void (^)(SecTrustResultType, BOOL))completionHandler {
  DCHECK(completionHandler);
  // SecTrustEvaluate performs trust evaluation synchronously, possibly making
  // network requests. The UI thread should not be blocked by that operation.
  bool dispatched = base::WorkerPool::PostTask(FROM_HERE, base::BindBlock(^{
    SecTrustResultType trustResult = kSecTrustResultInvalid;
    if (SecTrustEvaluate(trust.get(), &trustResult) != errSecSuccess) {
      trustResult = kSecTrustResultInvalid;
    }
    completionHandler(trustResult, YES);
  }), false /* task_is_slow */);

  if (!dispatched) {
    completionHandler(kSecTrustResultInvalid, NO);
  }
}

@end
