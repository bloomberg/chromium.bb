// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_host.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/browser_feature_extractor.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/safe_browsing/safebrowsing_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::ResourceRequestDetails;
using content::WebContents;

namespace safe_browsing {

namespace {

void EmptyUrlCheckCallback(bool processed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

}  // namespace

// This class is instantiated each time a new toplevel URL loads, and
// asynchronously checks whether the phishing classifier should run for this
// URL.  If so, it notifies the renderer with a StartPhishingDetection IPC.
// Objects of this class are ref-counted and will be destroyed once nobody
// uses it anymore.  If |web_contents|, |csd_service| or |host| go away you need
// to call Cancel().  We keep the |database_manager| alive in a ref pointer for
// as long as it takes.
class ClientSideDetectionHost::ShouldClassifyUrlRequest
    : public base::RefCountedThreadSafe<
          ClientSideDetectionHost::ShouldClassifyUrlRequest> {
 public:
  ShouldClassifyUrlRequest(const content::FrameNavigateParams& params,
                           WebContents* web_contents,
                           ClientSideDetectionService* csd_service,
                           SafeBrowsingDatabaseManager* database_manager,
                           ClientSideDetectionHost* host)
      : canceled_(false),
        params_(params),
        web_contents_(web_contents),
        csd_service_(csd_service),
        database_manager_(database_manager),
        host_(host) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(web_contents_);
    DCHECK(csd_service_);
    DCHECK(database_manager_);
    DCHECK(host_);
  }

  void Start() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // We start by doing some simple checks that can run on the UI thread.
    UMA_HISTOGRAM_COUNTS("SBClientPhishing.ClassificationStart", 1);

    // Only classify [X]HTML documents.
    if (params_.contents_mime_type != "text/html" &&
        params_.contents_mime_type != "application/xhtml+xml") {
      VLOG(1) << "Skipping phishing classification for URL: " << params_.url
              << " because it has an unsupported MIME type: "
              << params_.contents_mime_type;
      UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.PreClassificationCheckFail",
                                NO_CLASSIFY_UNSUPPORTED_MIME_TYPE,
                                NO_CLASSIFY_MAX);
      return;
    }

    if (csd_service_->IsPrivateIPAddress(params_.socket_address.host())) {
      VLOG(1) << "Skipping phishing classification for URL: " << params_.url
              << " because of hosting on private IP: "
              << params_.socket_address.host();
      UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.PreClassificationCheckFail",
                                NO_CLASSIFY_PRIVATE_IP,
                                NO_CLASSIFY_MAX);
      return;
    }

    // Don't run the phishing classifier if the tab is incognito.
    if (web_contents_->GetBrowserContext()->IsOffTheRecord()) {
      VLOG(1) << "Skipping phishing classification for URL: " << params_.url
              << " because we're browsing incognito.";
      UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.PreClassificationCheckFail",
                                NO_CLASSIFY_OFF_THE_RECORD,
                                NO_CLASSIFY_MAX);

      return;
    }

    // We lookup the csd-whitelist before we lookup the cache because
    // a URL may have recently been whitelisted.  If the URL matches
    // the csd-whitelist we won't start classification.  The
    // csd-whitelist check has to be done on the IO thread because it
    // uses the SafeBrowsing service class.
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ShouldClassifyUrlRequest::CheckCsdWhitelist,
                   this, params_.url));
  }

  void Cancel() {
    canceled_ = true;
    // Just to make sure we don't do anything stupid we reset all these
    // pointers except for the safebrowsing service class which may be
    // accessed by CheckCsdWhitelist().
    web_contents_ = NULL;
    csd_service_ = NULL;
    host_ = NULL;
  }

 private:
  friend class base::RefCountedThreadSafe<
      ClientSideDetectionHost::ShouldClassifyUrlRequest>;

  // Enum used to keep stats about why the pre-classification check failed.
  enum PreClassificationCheckFailures {
    OBSOLETE_NO_CLASSIFY_PROXY_FETCH,
    NO_CLASSIFY_PRIVATE_IP,
    NO_CLASSIFY_OFF_THE_RECORD,
    NO_CLASSIFY_MATCH_CSD_WHITELIST,
    NO_CLASSIFY_TOO_MANY_REPORTS,
    NO_CLASSIFY_UNSUPPORTED_MIME_TYPE,

    NO_CLASSIFY_MAX  // Always add new values before this one.
  };

  // The destructor can be called either from the UI or the IO thread.
  virtual ~ShouldClassifyUrlRequest() { }

  void CheckCsdWhitelist(const GURL& url) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!database_manager_ ||
        database_manager_->MatchCsdWhitelistUrl(url)) {
      // We're done.  There is no point in going back to the UI thread.
      VLOG(1) << "Skipping phishing classification for URL: " << url
              << " because it matches the csd whitelist";
      UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.PreClassificationCheckFail",
                                NO_CLASSIFY_MATCH_CSD_WHITELIST,
                                NO_CLASSIFY_MAX);
      return;
    }

    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ShouldClassifyUrlRequest::CheckCache, this));
  }

  void CheckCache() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (canceled_) {
      return;
    }

    // If result is cached, we don't want to run classification again
    bool is_phishing;
    if (csd_service_->GetValidCachedResult(params_.url, &is_phishing)) {
      VLOG(1) << "Satisfying request for " << params_.url << " from cache";
      UMA_HISTOGRAM_COUNTS("SBClientPhishing.RequestSatisfiedFromCache", 1);
      // Since we are already on the UI thread, this is safe.
      host_->MaybeShowPhishingWarning(params_.url, is_phishing);
      return;
    }

    // We want to limit the number of requests, though we will ignore the
    // limit for urls in the cache.  We don't want to start classifying
    // too many pages as phishing, but for those that we already think are
    // phishing we want to give ourselves a chance to fix false positives.
    if (csd_service_->IsInCache(params_.url)) {
      VLOG(1) << "Reporting limit skipped for " << params_.url
              << " as it was in the cache.";
      UMA_HISTOGRAM_COUNTS("SBClientPhishing.ReportLimitSkipped", 1);
    } else if (csd_service_->OverReportLimit()) {
      VLOG(1) << "Too many report phishing requests sent recently, "
              << "not running classification for " << params_.url;
      UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.PreClassificationCheckFail",
                                NO_CLASSIFY_TOO_MANY_REPORTS,
                                NO_CLASSIFY_MAX);
      return;
    }

    // Everything checks out, so start classification.
    // |web_contents_| is safe to call as we will be destructed
    // before it is.
    VLOG(1) << "Instruct renderer to start phishing detection for URL: "
            << params_.url;
    content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
    rvh->Send(new SafeBrowsingMsg_StartPhishingDetection(
        rvh->GetRoutingID(), params_.url));
  }

  // No need to protect |canceled_| with a lock because it is only read and
  // written by the UI thread.
  bool canceled_;
  content::FrameNavigateParams params_;
  WebContents* web_contents_;
  ClientSideDetectionService* csd_service_;
  // We keep a ref pointer here just to make sure the safe browsing
  // database manager stays alive long enough.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  ClientSideDetectionHost* host_;

  DISALLOW_COPY_AND_ASSIGN(ShouldClassifyUrlRequest);
};

// static
ClientSideDetectionHost* ClientSideDetectionHost::Create(
    WebContents* tab) {
  return new ClientSideDetectionHost(tab);
}

ClientSideDetectionHost::ClientSideDetectionHost(WebContents* tab)
    : content::WebContentsObserver(tab),
      csd_service_(NULL),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      unsafe_unique_page_id_(-1) {
  DCHECK(tab);
  // Note: csd_service_ and sb_service will be NULL here in testing.
  csd_service_ = g_browser_process->safe_browsing_detection_service();
  feature_extractor_.reset(new BrowserFeatureExtractor(tab, csd_service_));
  registrar_.Add(this, content::NOTIFICATION_RESOURCE_RESPONSE_STARTED,
                 content::Source<WebContents>(tab));

  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service) {
    ui_manager_ = sb_service->ui_manager();
    database_manager_ = sb_service->database_manager();
    ui_manager_->AddObserver(this);
  }
}

ClientSideDetectionHost::~ClientSideDetectionHost() {
  if (ui_manager_)
    ui_manager_->RemoveObserver(this);
}

bool ClientSideDetectionHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ClientSideDetectionHost, message)
    IPC_MESSAGE_HANDLER(SafeBrowsingHostMsg_PhishingDetectionDone,
                        OnPhishingDetectionDone)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ClientSideDetectionHost::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // TODO(noelutz): move this DCHECK to WebContents and fix all the unit tests
  // that don't call this method on the UI thread.
  // DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (details.is_in_page) {
    // If the navigation is within the same page, the user isn't really
    // navigating away.  We don't need to cancel a pending callback or
    // begin a new classification.
    return;
  }
  // If we navigate away and there currently is a pending phishing
  // report request we have to cancel it to make sure we don't display
  // an interstitial for the wrong page.  Note that this won't cancel
  // the server ping back but only cancel the showing of the
  // interstial.
  weak_factory_.InvalidateWeakPtrs();

  if (!csd_service_) {
    return;
  }

  // Cancel any pending classification request.
  if (classification_request_.get()) {
    classification_request_->Cancel();
  }
  browse_info_.reset(new BrowseInfo);

  // Store redirect chain information.
  if (params.url.host() != cur_host_) {
    cur_host_ = params.url.host();
    cur_host_redirects_ = params.redirects;
  }
  browse_info_->host_redirects = cur_host_redirects_;
  browse_info_->url_redirects = params.redirects;
  browse_info_->http_status_code = details.http_status_code;

  // Notify the renderer if it should classify this URL.
  classification_request_ = new ShouldClassifyUrlRequest(params,
                                                         web_contents(),
                                                         csd_service_,
                                                         database_manager_,
                                                         this);
  classification_request_->Start();
}

void ClientSideDetectionHost::OnSafeBrowsingHit(
    const SafeBrowsingUIManager::UnsafeResource& resource) {
  // Check that this notification is really for us and that it corresponds to
  // either a malware or phishing hit.  In this case we store the unique page
  // ID for later.
  if (web_contents() &&
      web_contents()->GetRenderProcessHost()->GetID() ==
          resource.render_process_host_id &&
      web_contents()->GetRenderViewHost()->GetRoutingID() ==
          resource.render_view_id &&
      (resource.threat_type == SB_THREAT_TYPE_URL_PHISHING ||
       resource.threat_type == SB_THREAT_TYPE_URL_MALWARE) &&
      web_contents()->GetController().GetActiveEntry()) {
    unsafe_unique_page_id_ =
        web_contents()->GetController().GetActiveEntry()->GetUniqueID();
    // We also keep the resource around in order to be able to send the
    // malicious URL to the server.
    unsafe_resource_.reset(new SafeBrowsingUIManager::UnsafeResource(resource));
    unsafe_resource_->callback.Reset();  // Don't do anything stupid.
  }
}

void ClientSideDetectionHost::WebContentsDestroyed(WebContents* tab) {
  DCHECK(tab);
  // Tell any pending classification request that it is being canceled.
  if (classification_request_.get()) {
    classification_request_->Cancel();
  }
  // Cancel all pending feature extractions.
  feature_extractor_.reset();
}

void ClientSideDetectionHost::OnPhishingDetectionDone(
    const std::string& verdict_str) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // There is something seriously wrong if there is no service class but
  // this method is called.  The renderer should not start phishing detection
  // if there isn't any service class in the browser.
  DCHECK(csd_service_);
  // There shouldn't be any pending requests because we revoke them everytime
  // we navigate away.
  DCHECK(!weak_factory_.HasWeakPtrs());
  DCHECK(browse_info_.get());

  // We parse the protocol buffer here.  If we're unable to parse it we won't
  // send the verdict further.
  scoped_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  if (csd_service_ &&
      !weak_factory_.HasWeakPtrs() &&
      browse_info_.get() &&
      verdict->ParseFromString(verdict_str) &&
      verdict->IsInitialized() &&
      // We only send the verdict to the server if the verdict is phishing or if
      // a SafeBrowsing interstitial was already shown for this site.  E.g., a
      // malware or phishing interstitial was shown but the user clicked
      // through.
      (verdict->is_phishing() || DidShowSBInterstitial())) {
    if (DidShowSBInterstitial()) {
      browse_info_->unsafe_resource.reset(unsafe_resource_.release());
    }
    // Start browser-side feature extraction.  Once we're done it will send
    // the client verdict request.
    feature_extractor_->ExtractFeatures(
        browse_info_.get(),
        verdict.release(),
        base::Bind(&ClientSideDetectionHost::FeatureExtractionDone,
                   weak_factory_.GetWeakPtr()));
  }
  browse_info_.reset();
}

void ClientSideDetectionHost::MaybeShowPhishingWarning(GURL phishing_url,
                                                       bool is_phishing) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(2) << "Received server phishing verdict for URL:" << phishing_url
          << " is_phishing:" << is_phishing;
  if (is_phishing) {
    DCHECK(web_contents());
    if (ui_manager_) {
      SafeBrowsingUIManager::UnsafeResource resource;
      resource.url = phishing_url;
      resource.original_url = phishing_url;
      resource.is_subresource = false;
      resource.threat_type = SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL;
      resource.render_process_host_id =
          web_contents()->GetRenderProcessHost()->GetID();
      resource.render_view_id =
          web_contents()->GetRenderViewHost()->GetRoutingID();
      if (!ui_manager_->IsWhitelisted(resource)) {
        // We need to stop any pending navigations, otherwise the interstital
        // might not get created properly.
        web_contents()->GetController().DiscardNonCommittedEntries();
        resource.callback = base::Bind(&EmptyUrlCheckCallback);
        ui_manager_->DoDisplayBlockingPage(resource);
      }
    }
  }
}

void ClientSideDetectionHost::FeatureExtractionDone(
    bool success,
    ClientPhishingRequest* request) {
  if (!request) {
    DLOG(FATAL) << "Invalid request object in FeatureExtractionDone";
    return;
  }
  VLOG(2) << "Feature extraction done (success:" << success << ") for URL: "
          << request->url() << ". Start sending client phishing request.";
  ClientSideDetectionService::ClientReportPhishingRequestCallback callback;
  // If the client-side verdict isn't phishing we don't care about the server
  // response because we aren't going to display a warning.
  if (request->is_phishing()) {
    callback = base::Bind(&ClientSideDetectionHost::MaybeShowPhishingWarning,
                          weak_factory_.GetWeakPtr());
  }
  // Send ping even if the browser feature extraction failed.
  csd_service_->SendClientReportPhishingRequest(
      request,  // The service takes ownership of the request object.
      callback);
}

void ClientSideDetectionHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(type, content::NOTIFICATION_RESOURCE_RESPONSE_STARTED);
  const ResourceRequestDetails* req = content::Details<ResourceRequestDetails>(
      details).ptr();
  if (req && browse_info_.get()) {
    browse_info_->ips.insert(req->socket_address.host());
  }
}

bool ClientSideDetectionHost::DidShowSBInterstitial() {
  if (unsafe_unique_page_id_ <= 0 || !web_contents()) {
    return false;
  }
  const NavigationEntry* nav_entry =
      web_contents()->GetController().GetActiveEntry();
  return (nav_entry && nav_entry->GetUniqueID() == unsafe_unique_page_id_);
}

void ClientSideDetectionHost::set_client_side_detection_service(
    ClientSideDetectionService* service) {
  csd_service_ = service;
}

void ClientSideDetectionHost::set_safe_browsing_managers(
    SafeBrowsingUIManager* ui_manager,
    SafeBrowsingDatabaseManager* database_manager) {
  if (ui_manager_)
    ui_manager_->RemoveObserver(this);

  ui_manager_ = ui_manager;
  if (ui_manager)
    ui_manager_->AddObserver(this);

  database_manager_ = database_manager;
}

}  // namespace safe_browsing
