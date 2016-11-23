// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/request_tracker_impl.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "base/bind_helpers.h"
#include "base/containers/hash_tables.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#import "ios/net/clients/crn_forwarding_network_client.h"
#import "ios/net/clients/crn_forwarding_network_client_factory.h"
#import "ios/web/crw_network_activity_indicator_manager.h"
#import "ios/web/history_state_util.h"
#import "ios/web/net/crw_request_tracker_delegate.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/certificate_policy_cache.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/url_util.h"
#include "ios/web/public/web_thread.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

struct EqualNSStrings {
  bool operator()(const base::scoped_nsobject<NSString>& s1,
                  const base::scoped_nsobject<NSString>& s2) const {
    // Use a ternary due to the BOOL vs bool type difference.
    return [s1 isEqualToString:s2] ? true : false;
  }
};

struct HashNSString {
  size_t operator()(const base::scoped_nsobject<NSString>& s) const {
    return [s hash];
  }
};

// A map of all RequestTrackerImpls for tabs that are:
// * Currently open
// * Recently closed waiting for all their network operations to finish.
// The code accesses this variable from two threads: the consumer is expected to
// always access it from the main thread, the provider is accessing it from the
// WebThread, a thread created by the UIWebView/CFURL. For this reason access to
// this variable must always gated by |g_trackers_lock|.
typedef base::hash_map<base::scoped_nsobject<NSString>,
                       web::RequestTrackerImpl*,
                       HashNSString, EqualNSStrings> TrackerMap;

TrackerMap* g_trackers = NULL;
base::Lock* g_trackers_lock = NULL;
pthread_once_t g_once_control = PTHREAD_ONCE_INIT;

// Flag, lock, and function to implement BlockUntilTrackersShutdown().
// |g_waiting_on_io_thread| is guarded by |g_waiting_on_io_thread_lock|;
// it is set to true when the shutdown wait starts, then a call to
// StopIOThreadWaiting is posted to the IO thread (enqueued after any pending
// request terminations) while the posting method loops over a check on the
// |g_waiting_on_io_thread|.
static bool g_waiting_on_io_thread = false;
base::Lock* g_waiting_on_io_thread_lock = NULL;
void StopIOThreadWaiting() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  base::AutoLock scoped_lock(*g_waiting_on_io_thread_lock);
  g_waiting_on_io_thread = false;
}

// Initialize global state. Calls to this should be conditional on
// |g_once_control| (that is, this should only be called once, across all
// threads).
void InitializeGlobals() {
  g_trackers = new TrackerMap;
  g_trackers_lock = new base::Lock;
  g_waiting_on_io_thread_lock = new base::Lock;
}

// Each request tracker get a unique increasing number, used anywhere an
// identifier is needed for tracker (e.g. storing certs).
int g_next_request_tracker_id = 0;

// IsIntranetHost logic and its associated kDot constant are lifted directly
// from content/browser/ssl/ssl_policy.cc. Unfortunately that particular file
// has way too many dependencies on content to be used on iOS.
static const char kDot = '.';

static bool IsIntranetHost(const std::string& host) {
  const size_t dot = host.find(kDot);
  return dot == std::string::npos || dot == host.length() - 1;
}

// Add |tracker| to |g_trackers| under |key|.
static void RegisterTracker(web::RequestTrackerImpl* tracker, NSString* key) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  pthread_once(&g_once_control, &InitializeGlobals);
  {
    base::scoped_nsobject<NSString> scoped_key([key copy]);
    base::AutoLock scoped_lock(*g_trackers_lock);
    DCHECK(!g_trackers->count(scoped_key));
    (*g_trackers)[scoped_key] = tracker;
  }
}

// Empty callback.
void DoNothing(bool flag) {}

}  // namespace

// The structure used to gather the information about the resources loaded.
struct TrackerCounts {
 public:
  TrackerCounts(const GURL& tracked_url, const net::URLRequest* tracked_request)
      : url(tracked_url),
        first_party_for_cookies_origin(
            tracked_request->first_party_for_cookies().GetOrigin()),
        request(tracked_request),
        ssl_info(net::SSLInfo()),
        ssl_judgment(web::CertPolicy::ALLOWED),
        allowed_by_user(false),
        expected_length(0),
        processed(0),
        done(false) {
    DCHECK_CURRENTLY_ON(web::WebThread::IO);
    is_subrequest = tracked_request->first_party_for_cookies().is_valid() &&
        tracked_request->url() != tracked_request->first_party_for_cookies();
  };

  // The resource url.
  const GURL url;
  // The origin of the url of the top level document of the resource. This is
  // used to ignore request coming from an old document when detecting mixed
  // content.
  const GURL first_party_for_cookies_origin;
  // The request associated with this struct. As a void* to prevent access from
  // the wrong thread.
  const void* request;
  // SSLInfo for the request.
  net::SSLInfo ssl_info;
  // Is the SSL request blocked waiting for user choice.
  web::CertPolicy::Judgment ssl_judgment;
  // True if |ssl_judgment| is ALLOWED as the result of a user choice.
  bool allowed_by_user;
  // block to call to cancel or authorize a blocked request.
  net::RequestTracker::SSLCallback ssl_callback;
  // If known, the expected length of the resource in bytes.
  uint64_t expected_length;
  // Number of bytes loaded so far.
  uint64_t processed;
  // Set to true is the resource is fully loaded.
  bool done;
  // Set to true if the request has a main request set.
  bool is_subrequest;

  NSString* Description() {
    NSString* spec = base::SysUTF8ToNSString(url.spec());
    NSString* status = nil;
    if (done) {
      status = [NSString stringWithFormat:@"\t-- Done -- (%04qu) bytes",
          processed];
    } else if (!expected_length) {
      status = [NSString stringWithFormat:@"\t>> Loading (%04qu) bytes",
          processed];
    } else {
      status = [NSString stringWithFormat:@"\t>> Loading (%04qu/%04qu)",
          processed, expected_length];
    }

    NSString* ssl = @"";
    if (ssl_info.is_valid()) {
      NSString* subject = base::SysUTF8ToNSString(
          ssl_info.cert.get()->subject().GetDisplayName());
      NSString* issuer = base::SysUTF8ToNSString(
          ssl_info.cert.get()->issuer().GetDisplayName());

      ssl = [NSString stringWithFormat:
          @"\n\t\tcert for '%@' issued by '%@'", subject, issuer];

      if (!net::IsCertStatusMinorError(ssl_info.cert_status)) {
        ssl = [NSString stringWithFormat:@"%@ (status: %0xd)",
            ssl, ssl_info.cert_status];
      }
    }
    return [NSString stringWithFormat:@"%@\n\t\t%@%@", status, spec, ssl];
  }

  DISALLOW_COPY_AND_ASSIGN(TrackerCounts);
};

// A SSL carrier is used to transport SSL information to the UI via its
// encapsulation in a block. Once the object is constructed all public methods
// can be called from any thread safely. This object is designed so it is
// instantiated on the IO thread but may be accessed from the UI thread.
@interface CRWSSLCarrier : NSObject {
 @private
  scoped_refptr<web::RequestTrackerImpl> tracker_;
  net::SSLInfo sslInfo_;
  GURL url_;
  web::SSLStatus status_;
}

// Designated initializer.
- (id)initWithTracker:(web::RequestTrackerImpl*)tracker
               counts:(const TrackerCounts*)counts;
// URL of the request.
- (const GURL&)url;
// Returns a SSLStatus representing the state of the page. This assumes the
// target carrier is the main page request.
- (const web::SSLStatus&)sslStatus;
// Returns a SSLInfo with a reference to the certificate and SSL information.
- (const net::SSLInfo&)sslInfo;
// Callback method to allow or deny the request from going through.
- (void)errorCallback:(BOOL)flag;
// Internal method used to build the SSLStatus object. Called from the
// initializer to make sure it is invoked on the network thread.
- (void)buildSSLStatus;
@end

@implementation CRWSSLCarrier

- (id)initWithTracker:(web::RequestTrackerImpl*)tracker
               counts:(const TrackerCounts*)counts {
  self = [super init];
  if (self) {
    tracker_ = tracker;
    url_ = counts->url;
    sslInfo_ = counts->ssl_info;
    [self buildSSLStatus];
  }
  return self;
}

- (const GURL&)url {
  return url_;
}

- (const net::SSLInfo&)sslInfo {
  return sslInfo_;
}

- (const web::SSLStatus&)sslStatus {
  return status_;
}

- (void)errorCallback:(BOOL)flag {
  base::scoped_nsobject<CRWSSLCarrier> scoped(self);
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE,
                           base::Bind(&web::RequestTrackerImpl::ErrorCallback,
                                      tracker_, scoped, flag));
}

- (void)buildSSLStatus {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (!sslInfo_.is_valid())
    return;

  status_.certificate = sslInfo_.cert;

  status_.cert_status = sslInfo_.cert_status;
  if (status_.cert_status & net::CERT_STATUS_COMMON_NAME_INVALID) {
    // CAs issue certificates for intranet hosts to everyone.  Therefore, we
    // mark intranet hosts as being non-unique.
    if (IsIntranetHost(url_.host())) {
      status_.cert_status |= net::CERT_STATUS_NON_UNIQUE_NAME;
    }
  }

  status_.security_bits = sslInfo_.security_bits;
  status_.connection_status = sslInfo_.connection_status;

  if (tracker_->has_mixed_content()) {
    // TODO(noyau): In iOS there is no notion of resource type. The insecure
    // content could be an image (DISPLAYED_INSECURE_CONTENT) or a script
    // (RAN_INSECURE_CONTENT). The status of the page is different for both, but
    // there is not enough information from UIWebView to differentiate the two
    // cases.
    status_.content_status = web::SSLStatus::DISPLAYED_INSECURE_CONTENT;
  } else {
    status_.content_status = web::SSLStatus::NORMAL_CONTENT;
  }

  if (!url_.SchemeIsCryptographic()) {
    // Should not happen as the sslInfo is valid.
    NOTREACHED();
    status_.security_style = web::SECURITY_STYLE_UNAUTHENTICATED;
  } else if (net::IsCertStatusError(status_.cert_status) &&
             !net::IsCertStatusMinorError(status_.cert_status)) {
    // Minor errors don't lower the security style to
    // SECURITY_STYLE_AUTHENTICATION_BROKEN.
    status_.security_style = web::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  } else {
    // This page is secure.
    status_.security_style = web::SECURITY_STYLE_AUTHENTICATED;
  }
}

- (NSString*)description {
  NSString* sslInfo = @"";
  if (sslInfo_.is_valid()) {
    switch (status_.security_style) {
      case web::SECURITY_STYLE_UNKNOWN:
      case web::SECURITY_STYLE_UNAUTHENTICATED:
        sslInfo = @"Unexpected SSL state ";
        break;
      case web::SECURITY_STYLE_AUTHENTICATION_BROKEN:
        sslInfo = @"Not secure ";
        break;
      case web::SECURITY_STYLE_WARNING:
        sslInfo = @"Security warning";
        break;
      case web::SECURITY_STYLE_AUTHENTICATED:
        if (status_.content_status ==
            web::SSLStatus::DISPLAYED_INSECURE_CONTENT)
          sslInfo = @"Mixed ";
        else
          sslInfo = @"Secure ";
        break;
    }
  }

  NSURL* url = net::NSURLWithGURL(url_);

  return [NSString stringWithFormat:@"<%@%@>", sslInfo, url];
}

@end

namespace web {

#pragma mark Consumer API

// static
scoped_refptr<RequestTrackerImpl>
RequestTrackerImpl::CreateTrackerForRequestGroupID(
    NSString* request_group_id,
    BrowserState* browser_state,
    net::URLRequestContextGetter* context_getter,
    id<CRWRequestTrackerDelegate> delegate) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(request_group_id);

  scoped_refptr<RequestTrackerImpl> tracker =
      new RequestTrackerImpl(request_group_id, context_getter, delegate);

  scoped_refptr<CertificatePolicyCache> policy_cache =
      BrowserState::GetCertificatePolicyCache(browser_state);
  DCHECK(policy_cache);

  // Take care of the IO-thread init.
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&RequestTrackerImpl::InitOnIOThread, tracker, policy_cache));
  RegisterTracker(tracker.get(), request_group_id);
  return tracker;
}

void RequestTrackerImpl::StartPageLoad(const GURL& url, id user_info) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  base::scoped_nsobject<id> scoped_user_info(user_info);
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&RequestTrackerImpl::TrimToURL, this, url, scoped_user_info));
}

void RequestTrackerImpl::FinishPageLoad(const GURL& url, bool load_success) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&RequestTrackerImpl::StopPageLoad, this, url, load_success));
}

void RequestTrackerImpl::HistoryStateChange(const GURL& url) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&RequestTrackerImpl::HistoryStateChangeToURL, this, url));
}

// Close is called when an owning object (a Tab or something that acts like
// it) is done with the RequestTrackerImpl. There may still be queued calls on
// the UI thread that will make use of the fields being cleaned-up here; they
// must ensure they they operate without crashing with the cleaned-up values.
void RequestTrackerImpl::Close() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  // Mark the tracker as closing on the IO thread. Note that because the local
  // scoped_refptr here retains |this|, we a are guaranteed that destruiction
  // won't begin until the block completes, and thus |is_closing_| will always
  // be set before destruction begins.
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE,
                           base::Bind(
                               [](RequestTrackerImpl* tracker) {
                                 tracker->is_closing_ = true;
                                 tracker->CancelRequests();
                               },
                               base::RetainedRef(this)));

  // Disable the delegate.
  delegate_ = nil;
  // The user_info is no longer needed.
  user_info_.reset();
}

// static
void RequestTrackerImpl::RunAfterRequestsCancel(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  // Post a no-op to the IO thread, and after that has executed, run |callback|.
  // This ensures that |callback| runs after anything elese queued on the IO
  // thread, in particular CancelRequest() calls made from closing trackers.
  web::WebThread::PostTaskAndReply(web::WebThread::IO, FROM_HERE,
                                   base::Bind(&base::DoNothing), callback);
}

// static
void RequestTrackerImpl::BlockUntilTrackersShutdown() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  {
    base::AutoLock scoped_lock(*g_waiting_on_io_thread_lock);
    g_waiting_on_io_thread = true;
  }
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE,
                           base::Bind(&StopIOThreadWaiting));

  // Poll endlessly until the wait flag is unset on the IO thread by
  // StopIOThreadWaiting().
  // (Consider instead having a hard time cap, like 100ms or so, after which
  // we stop blocking. In that case this method would return a boolean
  // indicating if the wait completed or not).
  while (1) {
    base::AutoLock scoped_lock(*g_waiting_on_io_thread_lock);
    if (!g_waiting_on_io_thread)
      return;
    // Ensure that other threads have a chance to run even on a single-core
    // devices.
    pthread_yield_np();
  }
}

#pragma mark Provider API

// static
RequestTrackerImpl* RequestTrackerImpl::GetTrackerForRequestGroupID(
    NSString* request_group_id) {
  DCHECK(request_group_id);
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  RequestTrackerImpl* tracker = nullptr;
  TrackerMap::iterator map_it;
  pthread_once(&g_once_control, &InitializeGlobals);
  {
    base::AutoLock scoped_lock(*g_trackers_lock);
    map_it = g_trackers->find(
        base::scoped_nsobject<NSString>([request_group_id copy]));
    if (map_it != g_trackers->end())
      tracker = map_it->second;
  }
  return tracker;
}

net::URLRequestContext* RequestTrackerImpl::GetRequestContext() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  return request_context_getter_->GetURLRequestContext();
}

void RequestTrackerImpl::StartRequest(net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(!counts_by_request_.count(request));
  DCHECK_EQ(is_for_static_file_requests_, request->url().SchemeIsFile());

  bool addedRequest = live_requests_.insert(request).second;
  if (!is_for_static_file_requests_ && addedRequest) {
    NSString* networkActivityKey = GetNetworkActivityKey();
    web::WebThread::PostTask(
        web::WebThread::UI, FROM_HERE, base::BindBlockArc(^{
          [[CRWNetworkActivityIndicatorManager sharedInstance]
              startNetworkTaskForGroup:networkActivityKey];
        }));
  }

  if (new_estimate_round_) {
    // Starting a new estimate round. Ignore the previous requests for the
    // calculation.
    counts_by_request_.clear();
    estimate_start_index_ = counts_.size();
    new_estimate_round_ = false;
  }
  const GURL& url = request->original_url();
  TrackerCounts* counts = new TrackerCounts(
      GURLByRemovingRefFromGURL(url), request);
  counts_.push_back(counts);
  counts_by_request_[request] = counts;
  if (page_url_.SchemeIsCryptographic() && !url.SchemeIsCryptographic())
    has_mixed_content_ = true;
  Notify();
}

void RequestTrackerImpl::CaptureHeaders(net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (is_closing_)
    return;

  if (!request->response_headers())
    return;

  scoped_refptr<net::HttpResponseHeaders> headers(request->response_headers());
  web::WebThread::PostTask(
      web::WebThread::UI, FROM_HERE,
      base::Bind(&RequestTrackerImpl::NotifyResponseHeaders, this,
                 base::RetainedRef(headers), request->url()));
}

void RequestTrackerImpl::CaptureExpectedLength(const net::URLRequest* request,
                                               uint64_t length) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (counts_by_request_.count(request)) {
    TrackerCounts* counts = counts_by_request_[request];
    DCHECK(!counts->done);
    if (length < counts->processed) {
      // Something is wrong with the estimate. Ignore it.
      counts->expected_length = 0;
    } else {
      counts->expected_length = length;
    }
    Notify();
  }
}

void RequestTrackerImpl::CaptureReceivedBytes(const net::URLRequest* request,
                                              uint64_t byte_count) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (counts_by_request_.count(request)) {
    TrackerCounts* counts = counts_by_request_[request];
    DCHECK(!counts->done);
    const net::SSLInfo& ssl_info = request->ssl_info();
    if (ssl_info.is_valid())
      counts->ssl_info = ssl_info;
    counts->processed += byte_count;
    if (counts->expected_length > 0 &&
        counts->expected_length < counts->processed) {
      // Something is wrong with the estimate, it is too low. Ignore it.
      counts->expected_length = 0;
    }
    Notify();
  }
}

void RequestTrackerImpl::StopRequest(net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  int removedRequests = live_requests_.erase(request);
  if (!is_for_static_file_requests_ && removedRequests > 0) {
    NSString* networkActivityKey = GetNetworkActivityKey();
    web::WebThread::PostTask(
        web::WebThread::UI, FROM_HERE, base::BindBlockArc(^{
          [[CRWNetworkActivityIndicatorManager sharedInstance]
              stopNetworkTaskForGroup:networkActivityKey];
        }));
  }

  if (counts_by_request_.count(request)) {
    StopRedirectedRequest(request);
    Notify();
  }
}

void RequestTrackerImpl::StopRedirectedRequest(net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  int removedRequests = live_requests_.erase(request);
  if (!is_for_static_file_requests_ && removedRequests > 0) {
    NSString* networkActivityKey = GetNetworkActivityKey();
    web::WebThread::PostTask(
        web::WebThread::UI, FROM_HERE, base::BindBlockArc(^{
          [[CRWNetworkActivityIndicatorManager sharedInstance]
              stopNetworkTaskForGroup:networkActivityKey];
        }));
  }

  if (counts_by_request_.count(request)) {
    TrackerCounts* counts = counts_by_request_[request];
    DCHECK(!counts->done);
    const net::SSLInfo& ssl_info = request->ssl_info();
    if (ssl_info.is_valid())
      counts->ssl_info = ssl_info;
    counts->done = true;
    counts_by_request_.erase(request);
  }
}

void RequestTrackerImpl::CaptureCertificatePolicyCache(
    const net::URLRequest* request,
    const RequestTracker::SSLCallback& should_continue) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  std::string host = request->url().host();
  CertPolicy::Judgment judgment = policy_cache_->QueryPolicy(
      request->ssl_info().cert.get(), host, request->ssl_info().cert_status);
  if (judgment == CertPolicy::UNKNOWN) {
    // The request comes from the cache, and has been loaded even though the
    // policy is UNKNOWN. Display the interstitial page now.
    OnSSLCertificateError(request, request->ssl_info(), true, should_continue);
    return;
  }

  // Notify the  delegate that a judgment has been used.
  DCHECK(judgment == CertPolicy::ALLOWED);
  if (counts_by_request_.count(request)) {
    const net::SSLInfo& ssl_info = request->ssl_info();
    TrackerCounts* counts = counts_by_request_[request];
    counts->allowed_by_user = true;
    if (ssl_info.is_valid())
      counts->ssl_info = ssl_info;
    web::WebThread::PostTask(
        web::WebThread::UI, FROM_HERE,
        base::Bind(&RequestTrackerImpl::NotifyCertificateUsed, this,
                   base::RetainedRef(ssl_info.cert), host,
                   ssl_info.cert_status));
  }
  should_continue.Run(true);
}

void RequestTrackerImpl::OnSSLCertificateError(
    const net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool recoverable,
    const RequestTracker::SSLCallback& should_continue) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(ssl_info.is_valid());

  if (counts_by_request_.count(request)) {
    TrackerCounts* counts = counts_by_request_[request];

    DCHECK(!counts->done);
    // Store the ssl error.
    counts->ssl_info = ssl_info;
    counts->ssl_callback = should_continue;
    counts->ssl_judgment =
        recoverable ? CertPolicy::UNKNOWN : CertPolicy::DENIED;
    ReevaluateCallbacksForAllCounts();
  }
}

void RequestTrackerImpl::ErrorCallback(CRWSSLCarrier* carrier, bool allow) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(policy_cache_);

  if (allow) {
    policy_cache_->AllowCertForHost([carrier sslInfo].cert.get(),
                                    [carrier url].host(),
                                    [carrier sslInfo].cert_status);
    ReevaluateCallbacksForAllCounts();
  }
  current_ssl_error_ = NULL;
}

#pragma mark Client utility methods.

void RequestTrackerImpl::PostUITaskIfOpen(const base::Closure& task) {
  PostTask(task, web::WebThread::UI);
}

// static
void RequestTrackerImpl::PostUITaskIfOpen(
    const base::WeakPtr<RequestTracker> tracker,
    const base::Closure& task) {
  if (!tracker)
    return;
  RequestTrackerImpl* tracker_impl =
      static_cast<RequestTrackerImpl*>(tracker.get());
  tracker_impl->PostUITaskIfOpen(task);
}

void RequestTrackerImpl::PostIOTask(const base::Closure& task) {
  PostTask(task, web::WebThread::IO);
}

void RequestTrackerImpl::ScheduleIOTask(const base::Closure& task) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE, task);
}

void RequestTrackerImpl::SetCacheModeFromUIThread(
    RequestTracker::CacheMode mode) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&RequestTracker::SetCacheMode, this, mode));
}

#pragma mark Private Object Lifecycle API

RequestTrackerImpl::RequestTrackerImpl(
    NSString* request_group_id,
    net::URLRequestContextGetter* context_getter,
    id<CRWRequestTrackerDelegate> delegate)
    : delegate_(delegate),
      previous_estimate_(0.0f),  // Not active by default.
      estimate_start_index_(0),
      notification_depth_(0),
      current_ssl_error_(NULL),
      has_mixed_content_(false),
      is_loading_(false),
      new_estimate_round_(true),
      is_for_static_file_requests_([delegate isForStaticFileRequests]),
      request_context_getter_(context_getter),
      identifier_(++g_next_request_tracker_id),
      request_group_id_([request_group_id copy]),
      is_closing_(false) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
}

void RequestTrackerImpl::InitOnIOThread(
    const scoped_refptr<CertificatePolicyCache>& policy_cache) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  Init();
  DCHECK(policy_cache);
  policy_cache_ = policy_cache;
}

RequestTrackerImpl::~RequestTrackerImpl() {
}

void RequestTrackerImplTraits::Destruct(const RequestTrackerImpl* t) {
  // RefCountedThreadSafe assumes we can do all the destruct tasks with a
  // const pointer, but we actually can't.
  RequestTrackerImpl* inconstant_t = const_cast<RequestTrackerImpl*>(t);
  if (web::WebThread::CurrentlyOn(web::WebThread::IO)) {
    inconstant_t->Destruct();
  } else {
    // Use BindBlock rather than Bind to avoid creating another scoped_refpter
    // to |this|. |inconstant_t| isn't retained by the block, but since this
    // method is the mechanism by which all RequestTrackerImpl instances are
    // destroyed, the object inconstant_t points to won't be deleted while
    // the block is executing (and Destruct() itself will do the deleting).
    web::WebThread::PostTask(web::WebThread::IO, FROM_HERE,
                             base::BindBlockArc(^{
                               inconstant_t->Destruct();
                             }));
  }
}

void RequestTrackerImpl::Destruct() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(is_closing_);

  pthread_once(&g_once_control, &InitializeGlobals);
  {
    base::AutoLock scoped_lock(*g_trackers_lock);
    g_trackers->erase(request_group_id_);
  }
  InvalidateWeakPtrs();
  // Delete on the UI thread.
  web::WebThread::PostTask(web::WebThread::UI, FROM_HERE, base::BindBlockArc(^{
                             delete this;
                           }));
}

#pragma mark Other private methods

void RequestTrackerImpl::Notify() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (is_closing_)
    return;
  // Notify() is called asynchronously, it runs later on the same
  // thread. This is used to collate notifications together, avoiding
  // blanketing the UI with a stream of information.
  notification_depth_ += 1;
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&RequestTrackerImpl::StackNotification, this));
}

void RequestTrackerImpl::StackNotification() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (is_closing_)
    return;

  // There is no point in sending the notification if there is another one
  // already queued. This queue is processing very lightweight changes and
  // should be exhausted very easily.
  --notification_depth_;
  if (notification_depth_)
    return;

  SSLNotify();
  if (is_loading_) {
    float estimate = EstimatedProgress();
    if (estimate != -1.0f) {
      web::WebThread::PostTask(
          web::WebThread::UI, FROM_HERE,
          base::Bind(&RequestTrackerImpl::NotifyUpdatedProgress, this,
                     estimate));
    }
  }
}

void RequestTrackerImpl::SSLNotify() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (is_closing_)
    return;

  if (counts_.empty())
    return;  // Nothing yet to notify.

  if (!page_url_.SchemeIsCryptographic())
    return;

  const GURL page_origin = page_url_.GetOrigin();
  ScopedVector<TrackerCounts>::iterator it;
  for (it = counts_.begin(); it != counts_.end(); ++it) {
    if (!(*it)->ssl_info.is_valid())
      continue;  // No SSL info at this point in time on this tracker.

    GURL request_origin = (*it)->url.GetOrigin();
    if (request_origin != page_origin)
      continue;  // Not interesting in the context of the page.

    base::scoped_nsobject<CRWSSLCarrier> carrier(
        [[CRWSSLCarrier alloc] initWithTracker:this counts:*it]);
    web::WebThread::PostTask(
        web::WebThread::UI, FROM_HERE,
        base::Bind(&RequestTrackerImpl::NotifyUpdatedSSLStatus, this, carrier));
    break;
  }
}

void RequestTrackerImpl::NotifyResponseHeaders(
    net::HttpResponseHeaders* headers,
    const GURL& request_url) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [delegate_ handleResponseHeaders:headers requestUrl:request_url];
}

void RequestTrackerImpl::NotifyCertificateUsed(
    net::X509Certificate* certificate,
    const std::string& host,
    net::CertStatus status) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [delegate_ certificateUsed:certificate forHost:host status:status];
}

void RequestTrackerImpl::NotifyUpdatedProgress(float estimate) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [delegate_ updatedProgress:estimate];
}

void RequestTrackerImpl::NotifyClearCertificates() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [delegate_ clearCertificates];
}

void RequestTrackerImpl::NotifyUpdatedSSLStatus(
    base::scoped_nsobject<CRWSSLCarrier> carrier) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [delegate_ updatedSSLStatus:[carrier sslStatus]
                   forPageUrl:[carrier url]
                     userInfo:user_info_];
}

void RequestTrackerImpl::NotifyPresentSSLError(
    base::scoped_nsobject<CRWSSLCarrier> carrier,
    bool recoverable) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  [delegate_ presentSSLError:[carrier sslInfo]
                forSSLStatus:[carrier sslStatus]
                       onUrl:[carrier url]
                 recoverable:recoverable
                    callback:^(BOOL flag) {
                         [carrier errorCallback:flag && recoverable];
                    }];
}

void RequestTrackerImpl::EvaluateSSLCallbackForCounts(TrackerCounts* counts) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(policy_cache_);

  // Ignore non-SSL requests.
  if (!counts->ssl_info.is_valid())
    return;

  CertPolicy::Judgment judgment =
      policy_cache_->QueryPolicy(counts->ssl_info.cert.get(),
                                 counts->url.host(),
                                 counts->ssl_info.cert_status);

  if (judgment != CertPolicy::ALLOWED) {
    // Apply some fine tuning.
    // TODO(droger): This logic is duplicated from SSLPolicy. Sharing the code
    // would be better.
    switch (net::MapCertStatusToNetError(counts->ssl_info.cert_status)) {
      case net::ERR_CERT_NO_REVOCATION_MECHANISM:
        // Ignore this error.
        judgment = CertPolicy::ALLOWED;
        break;
      case net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
        // We ignore this error but it will show a warning status in the
        // location bar.
        judgment = CertPolicy::ALLOWED;
        break;
      case net::ERR_CERT_CONTAINS_ERRORS:
      case net::ERR_CERT_REVOKED:
      case net::ERR_CERT_INVALID:
      case net::ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY:
      case net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN:
        judgment = CertPolicy::DENIED;
        break;
      case net::ERR_CERT_COMMON_NAME_INVALID:
      case net::ERR_CERT_DATE_INVALID:
      case net::ERR_CERT_AUTHORITY_INVALID:
      case net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
      case net::ERR_CERT_WEAK_KEY:
      case net::ERR_CERT_NAME_CONSTRAINT_VIOLATION:
      case net::ERR_CERT_VALIDITY_TOO_LONG:
        // Nothing. If DENIED it will stay denied. If UNKNOWN it will be
        // shown to the user for decision.
        break;
      default:
        NOTREACHED();
        judgment = CertPolicy::DENIED;
        break;
    }
  }

  counts->ssl_judgment = judgment;

  switch (judgment) {
    case CertPolicy::UNKNOWN:
    case CertPolicy::DENIED:
      // Simply cancel the request.
      CancelRequestForCounts(counts);
      break;
    case CertPolicy::ALLOWED:
      counts->ssl_callback.Run(YES);
      counts->ssl_callback = base::Bind(&DoNothing);
      break;
    default:
      NOTREACHED();
      // For now simply cancel the request.
      CancelRequestForCounts(counts);
      break;
  }
}

void RequestTrackerImpl::ReevaluateCallbacksForAllCounts() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (is_closing_)
    return;

  ScopedVector<TrackerCounts>::iterator it;
  for (it = counts_.begin(); it != counts_.end(); ++it) {
    // Check if the value hasn't changed via a user action.
    if ((*it)->ssl_judgment == CertPolicy::UNKNOWN)
      EvaluateSSLCallbackForCounts(*it);

    CertPolicy::Judgment judgment = (*it)->ssl_judgment;
    if (judgment == CertPolicy::ALLOWED)
      continue;

    // SSL errors on subrequests are simply ignored. The call to
    // EvaluateSSLCallbackForCounts() cancelled the request and nothing will
    // restart it.
    if ((*it)->is_subrequest)
      continue;

    if (!current_ssl_error_) {
      // For the UNKNOWN and DENIED state the information should be pushed to
      // the delegate. But only one at a time.
      current_ssl_error_ = (*it);
      base::scoped_nsobject<CRWSSLCarrier> carrier([[CRWSSLCarrier alloc]
          initWithTracker:this counts:current_ssl_error_]);
      web::WebThread::PostTask(
          web::WebThread::UI, FROM_HERE,
          base::Bind(&RequestTrackerImpl::NotifyPresentSSLError, this, carrier,
                     judgment == CertPolicy::UNKNOWN));
    }
  }
}

void RequestTrackerImpl::CancelRequestForCounts(TrackerCounts* counts) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  // Cancel the request.
  counts->done = true;
  counts_by_request_.erase(counts->request);
  counts->ssl_callback.Run(NO);
  counts->ssl_callback = base::Bind(&DoNothing);
  Notify();
}

PageCounts RequestTrackerImpl::pageCounts() {
  DCHECK_GE(counts_.size(), estimate_start_index_);

  PageCounts page_counts;

  ScopedVector<TrackerCounts>::iterator it;
  for (it = counts_.begin() + estimate_start_index_;
       it != counts_.end(); ++it) {
    if ((*it)->done) {
      uint64_t size = (*it)->processed;
      page_counts.finished += 1;
      page_counts.finished_bytes += size;
      if (page_counts.largest_byte_size_known < size) {
        page_counts.largest_byte_size_known = size;
      }
    } else {
      page_counts.unfinished += 1;
      if ((*it)->expected_length) {
        uint64_t size = (*it)->expected_length;
        page_counts.unfinished_estimate_bytes_done += (*it)->processed;
        page_counts.unfinished_estimated_bytes_left += size;
        if (page_counts.largest_byte_size_known < size) {
          page_counts.largest_byte_size_known = size;
        }
      } else {
        page_counts.unfinished_no_estimate += 1;
        page_counts.unfinished_no_estimate_bytes_done += (*it)->processed;
      }
    }
  }

  return page_counts;
}

float RequestTrackerImpl::EstimatedProgress() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  const PageCounts page_counts = pageCounts();

  // Nothing in progress and the last time was the same.
  if (!page_counts.unfinished && previous_estimate_ == 0.0f)
    return -1.0f;

  // First request.
  if (previous_estimate_ == 0.0f) {
    // start low.
    previous_estimate_ = 0.1f;
    return previous_estimate_;  // Return the just started status.
  }

  // The very simple case where everything is probably done and dusted.
  if (!page_counts.unfinished) {
    // Add 60%, and return. Another task is going to finish this.
    float bump = (1.0f - previous_estimate_) * 0.6f;
    previous_estimate_ += bump;
    return previous_estimate_;
  }

  // Calculate some ratios.
  // First the ratio of the finished vs the unfinished counts of resources
  // loaded.
  float unfinishedRatio =
      static_cast<float>(page_counts.finished) /
      static_cast<float>(page_counts.unfinished + page_counts.finished);

  // The ratio of bytes left vs bytes already downloaded for the resources where
  // no estimates of final size are known. For this ratio it is assumed the size
  // of a resource not downloaded yet is the maximum size of all the resources
  // seen so far.
  float noEstimateRatio = (!page_counts.unfinished_no_estimate_bytes_done) ?
      0.0f :
      static_cast<float>(page_counts.unfinished_no_estimate *
                         page_counts.largest_byte_size_known) /
          static_cast<float>(page_counts.finished_bytes +
                             page_counts.unfinished_no_estimate_bytes_done);

  // The ratio of bytes left vs bytes already downloaded for the resources with
  // available estimated size.
  float estimateRatio = (!page_counts.unfinished_estimated_bytes_left) ?
      noEstimateRatio :
      static_cast<float>(page_counts.unfinished_estimate_bytes_done) /
  static_cast<float>(page_counts.unfinished_estimate_bytes_done +
                     page_counts.unfinished_estimated_bytes_left);

  // Reassemble all of this.
  float total =
      0.1f +  // Minimum value.
      unfinishedRatio * 0.6f +
      estimateRatio * 0.3f;

  if (previous_estimate_ >= total)
    return -1.0f;

  // 10% of what's left.
  float maxBump = (1.0f - previous_estimate_) / 10.0f;
  // total is greater than previous estimate, need to bump the estimate up.
  if ((previous_estimate_ + maxBump) > total) {
    // Less than a 10% bump, bump to the new value.
    previous_estimate_ = total;
  } else {
    // Just bump by 10% toward the total.
    previous_estimate_ += maxBump;
  }

  return previous_estimate_;
}

void RequestTrackerImpl::RecomputeMixedContent(
    const TrackerCounts* split_position) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  // Check if the mixed content before trimming was correct.
  if (page_url_.SchemeIsCryptographic() && has_mixed_content_) {
    bool old_url_has_mixed_content = false;
    const GURL origin = page_url_.GetOrigin();
    ScopedVector<TrackerCounts>::iterator it = counts_.begin();
    while (it != counts_.end() && *it != split_position) {
      if (!(*it)->url.SchemeIsCryptographic() &&
          origin == (*it)->first_party_for_cookies_origin) {
        old_url_has_mixed_content = true;
        break;
      }
      ++it;
    }
    if (!old_url_has_mixed_content) {
      // We marked the previous page with incorrect data about its mixed
      // content. Turns out that the elements that triggered that condition
      // where in fact in a subsequent page. Duh.
      // Resend a notification for the |page_url_| informing the upper layer
      // that the mixed content was a red herring.
      has_mixed_content_ = false;
      SSLNotify();
    }
  }
}

void RequestTrackerImpl::RecomputeCertificatePolicy(
    const TrackerCounts* splitPosition) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  // Clear the judgments for the old URL.
  web::WebThread::PostTask(
      web::WebThread::UI, FROM_HERE,
      base::Bind(&RequestTrackerImpl::NotifyClearCertificates, this));
  // Report judgements for the new URL.
  ScopedVector<TrackerCounts>::const_reverse_iterator it;
  for (it = counts_.rbegin(); it != counts_.rend(); ++it) {
    TrackerCounts* counts = *it;
    if (counts->allowed_by_user) {
      std::string host = counts->url.host();
      web::WebThread::PostTask(
          web::WebThread::UI, FROM_HERE,
          base::Bind(&RequestTrackerImpl::NotifyCertificateUsed, this,
                     base::RetainedRef(counts->ssl_info.cert), host,
                     counts->ssl_info.cert_status));
    }
    if (counts == splitPosition)
      break;
  }
}

void RequestTrackerImpl::HistoryStateChangeToURL(const GURL& full_url) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  GURL url = GURLByRemovingRefFromGURL(full_url);

  if (is_loading_ &&
      web::history_state_util::IsHistoryStateChangeValid(url, page_url_)) {
    page_url_ = url;
  }
}

void RequestTrackerImpl::TrimToURL(const GURL& full_url, id user_info) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  GURL url = GURLByRemovingRefFromGURL(full_url);

  // Locate the request with this url, if present.
  bool new_url_has_mixed_content = false;
  bool url_scheme_is_secure = url.SchemeIsCryptographic();
  ScopedVector<TrackerCounts>::const_reverse_iterator rit = counts_.rbegin();
  while (rit != counts_.rend() && (*rit)->url != url) {
    if (url_scheme_is_secure && !(*rit)->url.SchemeIsCryptographic() &&
        (*rit)->first_party_for_cookies_origin == url.GetOrigin()) {
      new_url_has_mixed_content = true;
    }
    ++rit;
  }

  // |split_position| will be set to the count for the passed url if it exists.
  TrackerCounts* split_position = NULL;
  if (rit != counts_.rend()) {
    split_position = (*rit);
  } else {
    // The URL was not found, everything will be trimmed. The mixed content
    // calculation is invalid.
    new_url_has_mixed_content = false;

    // In the case of a page loaded via a HTML5 manifest there is no page
    // boundary to be found. However the latest count is a request for a
    // manifest. This tries to detect this peculiar case.
    // This is important as if this request for the manifest is on the same
    // domain as the page itself this will allow retrieval of the SSL
    // information.
    if (url_scheme_is_secure && counts_.size()) {
      TrackerCounts* back = counts_.back();
      const GURL& back_url = back->url;
      if (back_url.SchemeIsCryptographic() &&
          back_url.GetOrigin() == url.GetOrigin() && !back->is_subrequest) {
        split_position = back;
      }
    }
  }
  RecomputeMixedContent(split_position);
  RecomputeCertificatePolicy(split_position);

  // Trim up to that element.
  ScopedVector<TrackerCounts>::iterator it = counts_.begin();
  while (it != counts_.end() && *it != split_position) {
    if (!(*it)->done) {
      // This is for an unfinished request on a previous page. We do not care
      // about those anymore. Cancel the request.
      if ((*it)->ssl_judgment == CertPolicy::UNKNOWN)
        CancelRequestForCounts(*it);
      counts_by_request_.erase((*it)->request);
    }
    it = counts_.erase(it);
  }

  has_mixed_content_ = new_url_has_mixed_content;
  page_url_ = url;
  user_info_.reset(user_info);
  estimate_start_index_ = 0;
  is_loading_ = true;
  previous_estimate_ = 0.0f;
  new_estimate_round_ = true;
  ReevaluateCallbacksForAllCounts();
  Notify();
}

void RequestTrackerImpl::StopPageLoad(const GURL& url, bool load_success) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(page_url_ == GURLByRemovingRefFromGURL(url));
  is_loading_ = false;
}

#pragma mark Internal utilities for task posting

void RequestTrackerImpl::PostTask(const base::Closure& task,
                                  web::WebThread::ID thread) {
  // Absolute sanity test: |thread| is one of {UI, IO}
  DCHECK(thread == web::WebThread::UI || thread == web::WebThread::IO);
  // Check that we're on the counterpart thread to the one we're posting to.
  DCHECK_CURRENTLY_ON(thread == web::WebThread::IO ? web::WebThread::UI
                                                   : web::WebThread::IO);
  // Don't post if the tracker is closing and we're on the IO thread.
  // (there should be no way to call anything from the UI thread if
  // the tracker is closing).
  if (is_closing_ && web::WebThread::CurrentlyOn(web::WebThread::IO))
    return;
  web::WebThread::PostTask(thread, FROM_HERE, task);
}

#pragma mark Other internal methods.

NSString* RequestTrackerImpl::UnsafeDescription() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  NSMutableArray* urls = [NSMutableArray array];
  ScopedVector<TrackerCounts>::iterator it;
  for (it = counts_.begin(); it != counts_.end(); ++it)
    [urls addObject:(*it)->Description()];

  return [NSString stringWithFormat:@"RequestGroupID %@\n%@\n%@",
                                    request_group_id_.get(),
                                    net::NSURLWithGURL(page_url_),
                                    [urls componentsJoinedByString:@"\n"]];
}

NSString* RequestTrackerImpl::GetNetworkActivityKey() {
  return [NSString
      stringWithFormat:@"RequestTrackerImpl.NetworkActivityIndicatorKey.%@",
                       request_group_id_.get()];
  }

void RequestTrackerImpl::CancelRequests() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  std::set<net::URLRequest*>::iterator it;
  // TODO(droger): When canceling the request, we should in theory make sure
  // that the NSURLProtocol client method |didFailWithError| is called,
  // otherwise the iOS system may wait indefinitely for the request to complete.
  // However, as we currently only cancel the requests when closing a tab, the
  // requests are all canceled by the system shortly after and nothing bad
  // happens.
  for (it = live_requests_.begin(); it != live_requests_.end(); ++it)
    (*it)->Cancel();

  int removedRequests = live_requests_.size();
  live_requests_.clear();
  if (!is_for_static_file_requests_ && removedRequests > 0) {
    NSString* networkActivityKey = GetNetworkActivityKey();
    web::WebThread::PostTask(
        web::WebThread::UI, FROM_HERE, base::BindBlockArc(^{
          [[CRWNetworkActivityIndicatorManager sharedInstance]
              clearNetworkTasksForGroup:networkActivityKey];
        }));
  }
}

void RequestTrackerImpl::SetCertificatePolicyCacheForTest(
    web::CertificatePolicyCache* cache) {
  policy_cache_ = cache;
}

}  // namespace web
