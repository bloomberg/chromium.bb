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
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/browser_feature_extractor.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
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
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::ResourceRequestDetails;
using content::ResourceType;
using content::WebContents;

namespace safe_browsing {

const size_t ClientSideDetectionHost::kMaxUrlsPerIP = 20;
const size_t ClientSideDetectionHost::kMaxIPsPerBrowse = 200;

const char kSafeBrowsingMatchKey[] = "safe_browsing_match";

typedef base::Callback<void(bool)> ShouldClassifyUrlCallback;

// This class is instantiated each time a new toplevel URL loads, and
// asynchronously checks whether the malware and phishing classifiers should run
// for this URL.  If so, it notifies the host class by calling the provided
// callback form the UI thread.  Objects of this class are ref-counted and will
// be destroyed once nobody uses it anymore.  If |web_contents|, |csd_service|
// or |host| go away you need to call Cancel().  We keep the |database_manager|
// alive in a ref pointer for as long as it takes.
class ClientSideDetectionHost::ShouldClassifyUrlRequest
    : public base::RefCountedThreadSafe<
          ClientSideDetectionHost::ShouldClassifyUrlRequest> {
 public:
  ShouldClassifyUrlRequest(
      const content::FrameNavigateParams& params,
      const ShouldClassifyUrlCallback& start_phishing_classification,
      const ShouldClassifyUrlCallback& start_malware_classification,
      WebContents* web_contents,
      ClientSideDetectionService* csd_service,
      SafeBrowsingDatabaseManager* database_manager,
      ClientSideDetectionHost* host)
      : params_(params),
        web_contents_(web_contents),
        csd_service_(csd_service),
        database_manager_(database_manager),
        host_(host),
        start_phishing_classification_cb_(start_phishing_classification),
        start_malware_classification_cb_(start_malware_classification) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(web_contents_);
    DCHECK(csd_service_);
    DCHECK(database_manager_.get());
    DCHECK(host_);
  }

  void Start() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // We start by doing some simple checks that can run on the UI thread.
    UMA_HISTOGRAM_BOOLEAN("SBClientPhishing.ClassificationStart", 1);
    UMA_HISTOGRAM_BOOLEAN("SBClientMalware.ClassificationStart", 1);

    // Only classify [X]HTML documents.
    if (params_.contents_mime_type != "text/html" &&
        params_.contents_mime_type != "application/xhtml+xml") {
      VLOG(1) << "Skipping phishing classification for URL: " << params_.url
              << " because it has an unsupported MIME type: "
              << params_.contents_mime_type;
      DontClassifyForPhishing(NO_CLASSIFY_UNSUPPORTED_MIME_TYPE);
    }

    if (csd_service_->IsPrivateIPAddress(params_.socket_address.host())) {
      VLOG(1) << "Skipping phishing classification for URL: " << params_.url
              << " because of hosting on private IP: "
              << params_.socket_address.host();
      DontClassifyForPhishing(NO_CLASSIFY_PRIVATE_IP);
      DontClassifyForMalware(NO_CLASSIFY_PRIVATE_IP);
    }

    // For phishing we only classify HTTP pages.
    if (!params_.url.SchemeIs(url::kHttpScheme)) {
      VLOG(1) << "Skipping phishing classification for URL: " << params_.url
              << " because it is not HTTP: "
              << params_.socket_address.host();
      DontClassifyForPhishing(NO_CLASSIFY_NOT_HTTP_URL);
    }

    // Don't run any classifier if the tab is incognito.
    if (web_contents_->GetBrowserContext()->IsOffTheRecord()) {
      VLOG(1) << "Skipping phishing and malware classification for URL: "
              << params_.url << " because we're browsing incognito.";
      DontClassifyForPhishing(NO_CLASSIFY_OFF_THE_RECORD);
      DontClassifyForMalware(NO_CLASSIFY_OFF_THE_RECORD);
    }

    // We lookup the csd-whitelist before we lookup the cache because
    // a URL may have recently been whitelisted.  If the URL matches
    // the csd-whitelist we won't start phishing classification.  The
    // csd-whitelist check has to be done on the IO thread because it
    // uses the SafeBrowsing service class.
    if (ShouldClassifyForPhishing() || ShouldClassifyForMalware()) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(&ShouldClassifyUrlRequest::CheckSafeBrowsingDatabase,
                     this, params_.url));
    }
  }

  void Cancel() {
    DontClassifyForPhishing(NO_CLASSIFY_CANCEL);
    DontClassifyForMalware(NO_CLASSIFY_CANCEL);
    // Just to make sure we don't do anything stupid we reset all these
    // pointers except for the safebrowsing service class which may be
    // accessed by CheckSafeBrowsingDatabase().
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
    NO_CLASSIFY_NO_DATABASE_MANAGER,
    NO_CLASSIFY_KILLSWITCH,
    NO_CLASSIFY_CANCEL,
    NO_CLASSIFY_RESULT_FROM_CACHE,
    NO_CLASSIFY_NOT_HTTP_URL,

    NO_CLASSIFY_MAX  // Always add new values before this one.
  };

  // The destructor can be called either from the UI or the IO thread.
  virtual ~ShouldClassifyUrlRequest() { }

  bool ShouldClassifyForPhishing() const {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return !start_phishing_classification_cb_.is_null();
  }

  bool ShouldClassifyForMalware() const {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return !start_malware_classification_cb_.is_null();
  }

  void DontClassifyForPhishing(PreClassificationCheckFailures reason) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (ShouldClassifyForPhishing()) {
      // Track the first reason why we stopped classifying for phishing.
      UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.PreClassificationCheckFail",
                                reason, NO_CLASSIFY_MAX);
      DVLOG(2) << "Failed phishing pre-classification checks.  Reason: "
               << reason;
      start_phishing_classification_cb_.Run(false);
    }
    start_phishing_classification_cb_.Reset();
  }

  void DontClassifyForMalware(PreClassificationCheckFailures reason) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (ShouldClassifyForMalware()) {
      // Track the first reason why we stopped classifying for malware.
      UMA_HISTOGRAM_ENUMERATION("SBClientMalware.PreClassificationCheckFail",
                                reason, NO_CLASSIFY_MAX);
      DVLOG(2) << "Failed malware pre-classification checks.  Reason: "
               << reason;
      start_malware_classification_cb_.Run(false);
    }
    start_malware_classification_cb_.Reset();
  }

  void CheckSafeBrowsingDatabase(const GURL& url) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    // We don't want to call the classification callbacks from the IO
    // thread so we simply pass the results of this method to CheckCache()
    // which is called on the UI thread;
    PreClassificationCheckFailures phishing_reason = NO_CLASSIFY_MAX;
    PreClassificationCheckFailures malware_reason = NO_CLASSIFY_MAX;
    if (!database_manager_.get()) {
      // We cannot check the Safe Browsing whitelists so we stop here
      // for safety.
      malware_reason = phishing_reason = NO_CLASSIFY_NO_DATABASE_MANAGER;
    } else {
      if (database_manager_->MatchCsdWhitelistUrl(url)) {
        VLOG(1) << "Skipping phishing classification for URL: " << url
                << " because it matches the csd whitelist";
        phishing_reason = NO_CLASSIFY_MATCH_CSD_WHITELIST;
      }
      if (database_manager_->IsMalwareKillSwitchOn()) {
        malware_reason = NO_CLASSIFY_KILLSWITCH;
      }
    }
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ShouldClassifyUrlRequest::CheckCache,
                   this,
                   phishing_reason,
                   malware_reason));
  }

  void CheckCache(PreClassificationCheckFailures phishing_reason,
                  PreClassificationCheckFailures malware_reason) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (phishing_reason != NO_CLASSIFY_MAX)
      DontClassifyForPhishing(phishing_reason);
    if (malware_reason != NO_CLASSIFY_MAX)
      DontClassifyForMalware(malware_reason);
    if (!ShouldClassifyForMalware() && !ShouldClassifyForPhishing()) {
      return;  // No point in doing anything else.
    }
    // If result is cached, we don't want to run classification again.
    // In that case we're just trying to show the warning.
    bool is_phishing;
    if (csd_service_->GetValidCachedResult(params_.url, &is_phishing)) {
      VLOG(1) << "Satisfying request for " << params_.url << " from cache";
      UMA_HISTOGRAM_BOOLEAN("SBClientPhishing.RequestSatisfiedFromCache", 1);
      // Since we are already on the UI thread, this is safe.
      host_->MaybeShowPhishingWarning(params_.url, is_phishing);
      DontClassifyForPhishing(NO_CLASSIFY_RESULT_FROM_CACHE);
    }

    // We want to limit the number of requests, though we will ignore the
    // limit for urls in the cache.  We don't want to start classifying
    // too many pages as phishing, but for those that we already think are
    // phishing we want to send a request to the server to give ourselves
    // a chance to fix misclassifications.
    if (csd_service_->IsInCache(params_.url)) {
      VLOG(1) << "Reporting limit skipped for " << params_.url
              << " as it was in the cache.";
      UMA_HISTOGRAM_BOOLEAN("SBClientPhishing.ReportLimitSkipped", 1);
    } else if (csd_service_->OverPhishingReportLimit()) {
      VLOG(1) << "Too many report phishing requests sent recently, "
              << "not running classification for " << params_.url;
      DontClassifyForPhishing(NO_CLASSIFY_TOO_MANY_REPORTS);
    }
    if (csd_service_->OverMalwareReportLimit()) {
      DontClassifyForMalware(NO_CLASSIFY_TOO_MANY_REPORTS);
    }

    // Everything checks out, so start classification.
    // |web_contents_| is safe to call as we will be destructed
    // before it is.
    if (ShouldClassifyForPhishing()) {
      start_phishing_classification_cb_.Run(true);
      // Reset the callback to make sure ShouldClassifyForPhishing()
      // returns false.
      start_phishing_classification_cb_.Reset();
    }
    if (ShouldClassifyForMalware()) {
      start_malware_classification_cb_.Run(true);
      // Reset the callback to make sure ShouldClassifyForMalware()
      // returns false.
      start_malware_classification_cb_.Reset();
    }
  }

  content::FrameNavigateParams params_;
  WebContents* web_contents_;
  ClientSideDetectionService* csd_service_;
  // We keep a ref pointer here just to make sure the safe browsing
  // database manager stays alive long enough.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  ClientSideDetectionHost* host_;

  ShouldClassifyUrlCallback start_phishing_classification_cb_;
  ShouldClassifyUrlCallback start_malware_classification_cb_;

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
      classification_request_(NULL),
      should_extract_malware_features_(true),
      should_classify_for_malware_(false),
      pageload_complete_(false),
      weak_factory_(this),
      unsafe_unique_page_id_(-1) {
  DCHECK(tab);
  // Note: csd_service_ and sb_service will be NULL here in testing.
  csd_service_ = g_browser_process->safe_browsing_detection_service();
  feature_extractor_.reset(new BrowserFeatureExtractor(tab, this));
  registrar_.Add(this, content::NOTIFICATION_RESOURCE_RESPONSE_STARTED,
                 content::Source<WebContents>(tab));

  scoped_refptr<SafeBrowsingService> sb_service =
      g_browser_process->safe_browsing_service();
  if (sb_service.get()) {
    ui_manager_ = sb_service->ui_manager();
    database_manager_ = sb_service->database_manager();
    ui_manager_->AddObserver(this);
  }
}

ClientSideDetectionHost::~ClientSideDetectionHost() {
  if (ui_manager_.get())
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
  // Cancel any pending classification request.
  if (classification_request_.get()) {
    classification_request_->Cancel();
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
  browse_info_.reset(new BrowseInfo);

  // Store redirect chain information.
  if (params.url.host() != cur_host_) {
    cur_host_ = params.url.host();
    cur_host_redirects_ = params.redirects;
  }
  browse_info_->url = params.url;
  browse_info_->host_redirects = cur_host_redirects_;
  browse_info_->url_redirects = params.redirects;
  browse_info_->referrer = params.referrer.url;
  browse_info_->http_status_code = details.http_status_code;
  browse_info_->page_id = params.page_id;

  should_extract_malware_features_ = true;
  should_classify_for_malware_ = false;
  pageload_complete_ = false;

  // Check whether we can cassify the current URL for phishing or malware.
  classification_request_ = new ShouldClassifyUrlRequest(
      params,
      base::Bind(&ClientSideDetectionHost::OnPhishingPreClassificationDone,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ClientSideDetectionHost::OnMalwarePreClassificationDone,
                 weak_factory_.GetWeakPtr()),
      web_contents(), csd_service_, database_manager_.get(), this);
  classification_request_->Start();
}

void ClientSideDetectionHost::OnSafeBrowsingHit(
    const SafeBrowsingUIManager::UnsafeResource& resource) {
  if (!web_contents() || !web_contents()->GetController().GetActiveEntry())
    return;

  // Check that the hit is either malware or phishing.
  if (resource.threat_type != SB_THREAT_TYPE_URL_PHISHING &&
      resource.threat_type != SB_THREAT_TYPE_URL_MALWARE)
    return;

  // Check that this notification is really for us.
  content::RenderViewHost* hit_rvh = content::RenderViewHost::FromID(
      resource.render_process_host_id, resource.render_view_id);
  if (!hit_rvh ||
      web_contents() != content::WebContents::FromRenderViewHost(hit_rvh))
    return;

  // Store the unique page ID for later.
  unsafe_unique_page_id_ =
      web_contents()->GetController().GetActiveEntry()->GetUniqueID();

  // We also keep the resource around in order to be able to send the
  // malicious URL to the server.
  unsafe_resource_.reset(new SafeBrowsingUIManager::UnsafeResource(resource));
  unsafe_resource_->callback.Reset();  // Don't do anything stupid.
}

void ClientSideDetectionHost::OnSafeBrowsingMatch(
    const SafeBrowsingUIManager::UnsafeResource& resource) {
  if (!web_contents() || !web_contents()->GetController().GetActiveEntry())
    return;

  // Check that this notification is really for us.
  content::RenderViewHost* hit_rvh = content::RenderViewHost::FromID(
      resource.render_process_host_id, resource.render_view_id);
  if (!hit_rvh ||
      web_contents() != content::WebContents::FromRenderViewHost(hit_rvh))
    return;

  web_contents()->GetController().GetActiveEntry()->SetExtraData(
      kSafeBrowsingMatchKey, base::ASCIIToUTF16("1"));
}

scoped_refptr<SafeBrowsingDatabaseManager>
ClientSideDetectionHost::database_manager() {
  return database_manager_;
}

bool ClientSideDetectionHost::DidPageReceiveSafeBrowsingMatch() const {
  if (!web_contents() || !web_contents()->GetController().GetVisibleEntry())
    return false;

  // If an interstitial page is showing, GetVisibleEntry will return the
  // transient NavigationEntry for the interstitial. The transient entry
  // will not have the flag set, so use the pending entry instead if there
  // is one.
  NavigationEntry* entry = web_contents()->GetController().GetPendingEntry();
  if (!entry) {
    entry = web_contents()->GetController().GetVisibleEntry();
    if (entry->GetPageType() == content::PAGE_TYPE_INTERSTITIAL)
      entry = web_contents()->GetController().GetLastCommittedEntry();
    if (!entry)
      return false;
  }

  base::string16 value;
  return entry->GetExtraData(kSafeBrowsingMatchKey, &value);
}

void ClientSideDetectionHost::WebContentsDestroyed() {
  // Tell any pending classification request that it is being canceled.
  if (classification_request_.get()) {
    classification_request_->Cancel();
  }
  // Cancel all pending feature extractions.
  feature_extractor_.reset();
}

void ClientSideDetectionHost::OnPhishingPreClassificationDone(
    bool should_classify) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (browse_info_.get() && should_classify) {
    VLOG(1) << "Instruct renderer to start phishing detection for URL: "
            << browse_info_->url;
    content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
    rvh->Send(new SafeBrowsingMsg_StartPhishingDetection(
        rvh->GetRoutingID(), browse_info_->url));
  }
}

void ClientSideDetectionHost::OnMalwarePreClassificationDone(
    bool should_classify) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If classification checks failed we should stop extracting malware features.
  DVLOG(2) << "Malware pre-classification checks done. Should classify: "
           << should_classify;
  should_extract_malware_features_ = should_classify;
  should_classify_for_malware_ = should_classify;
  MaybeStartMalwareFeatureExtraction();
}

void ClientSideDetectionHost::DidStopLoading(content::RenderViewHost* rvh) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!csd_service_ || !browse_info_.get())
    return;
  DVLOG(2) << "Page finished loading.";
  pageload_complete_ = true;
  MaybeStartMalwareFeatureExtraction();
}

void ClientSideDetectionHost::MaybeStartMalwareFeatureExtraction() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (csd_service_ && browse_info_.get() &&
      should_classify_for_malware_ &&
      pageload_complete_) {
    scoped_ptr<ClientMalwareRequest> malware_request(
        new ClientMalwareRequest);
    // Start browser-side malware feature extraction.  Once we're done it will
    // send the malware client verdict request.
    malware_request->set_url(browse_info_->url.spec());
    const GURL& referrer = browse_info_->referrer;
    if (referrer.SchemeIs("http")) {  // Only send http urls.
      malware_request->set_referrer_url(referrer.spec());
    }
    // This function doesn't expect browse_info_ to stay around after this
    // function returns.
    feature_extractor_->ExtractMalwareFeatures(
        browse_info_.get(),
        malware_request.release(),
        base::Bind(&ClientSideDetectionHost::MalwareFeatureExtractionDone,
                   weak_factory_.GetWeakPtr()));
    should_classify_for_malware_ = false;
  }
}

void ClientSideDetectionHost::OnPhishingDetectionDone(
    const std::string& verdict_str) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // There is something seriously wrong if there is no service class but
  // this method is called.  The renderer should not start phishing detection
  // if there isn't any service class in the browser.
  DCHECK(csd_service_);
  DCHECK(browse_info_.get());

  // We parse the protocol buffer here.  If we're unable to parse it we won't
  // send the verdict further.
  scoped_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  if (csd_service_ &&
      browse_info_.get() &&
      verdict->ParseFromString(verdict_str) &&
      verdict->IsInitialized()) {
    // We only send phishing verdict to the server if the verdict is phishing or
    // if a SafeBrowsing interstitial was already shown for this site.  E.g., a
    // malware or phishing interstitial was shown but the user clicked
    // through.
    if (verdict->is_phishing() || DidShowSBInterstitial()) {
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
  }
}

void ClientSideDetectionHost::MaybeShowPhishingWarning(GURL phishing_url,
                                                       bool is_phishing) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(2) << "Received server phishing verdict for URL:" << phishing_url
           << " is_phishing:" << is_phishing;
  if (is_phishing) {
    DCHECK(web_contents());
    if (ui_manager_.get()) {
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
      }
      ui_manager_->DisplayBlockingPage(resource);
    }
    // If there is true phishing verdict, invalidate weakptr so that no longer
    // consider the malware vedict.
    weak_factory_.InvalidateWeakPtrs();
  }
}

void ClientSideDetectionHost::MaybeShowMalwareWarning(GURL original_url,
                                                      GURL malware_url,
                                                      bool is_malware) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(2) << "Received server malawre IP verdict for URL:" << malware_url
           << " is_malware:" << is_malware;
  if (is_malware && malware_url.is_valid() && original_url.is_valid()) {
    DCHECK(web_contents());
    if (ui_manager_.get()) {
      SafeBrowsingUIManager::UnsafeResource resource;
      resource.url = malware_url;
      resource.original_url = original_url;
      resource.is_subresource = (malware_url.host() != original_url.host());
      resource.threat_type = SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL;
      resource.render_process_host_id =
          web_contents()->GetRenderProcessHost()->GetID();
      resource.render_view_id =
          web_contents()->GetRenderViewHost()->GetRoutingID();
      if (!ui_manager_->IsWhitelisted(resource)) {
        // We need to stop any pending navigations, otherwise the interstital
        // might not get created properly.
        web_contents()->GetController().DiscardNonCommittedEntries();
      }
      ui_manager_->DisplayBlockingPage(resource);
    }
    // If there is true malware verdict, invalidate weakptr so that no longer
    // consider the phishing vedict.
    weak_factory_.InvalidateWeakPtrs();
  }
}

void ClientSideDetectionHost::FeatureExtractionDone(
    bool success,
    scoped_ptr<ClientPhishingRequest> request) {
  DCHECK(request);
  DVLOG(2) << "Feature extraction done (success:" << success << ") for URL: "
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
      request.release(),  // The service takes ownership of the request object.
      callback);
}

void ClientSideDetectionHost::MalwareFeatureExtractionDone(
    bool feature_extraction_success,
    scoped_ptr<ClientMalwareRequest> request) {
  DCHECK(request.get());
  DVLOG(2) << "Malware Feature extraction done for URL: " << request->url()
           << ", with badip url count:" << request->bad_ip_url_info_size();

  // Send ping if there is matching features.
  if (feature_extraction_success && request->bad_ip_url_info_size() > 0) {
    VLOG(1) << "Start sending client malware request.";
    ClientSideDetectionService::ClientReportMalwareRequestCallback callback;
    callback = base::Bind(&ClientSideDetectionHost::MaybeShowMalwareWarning,
                          weak_factory_.GetWeakPtr());
    csd_service_->SendClientReportMalwareRequest(request.release(), callback);
  }
}

void ClientSideDetectionHost::UpdateIPUrlMap(const std::string& ip,
                                             const std::string& url,
                                             const std::string& method,
                                             const std::string& referrer,
                                             const ResourceType resource_type) {
  if (ip.empty() || url.empty())
    return;

  IPUrlMap::iterator it = browse_info_->ips.find(ip);
  if (it == browse_info_->ips.end()) {
    if (browse_info_->ips.size() < kMaxIPsPerBrowse) {
      std::vector<IPUrlInfo> url_infos;
      url_infos.push_back(IPUrlInfo(url, method, referrer, resource_type));
      browse_info_->ips.insert(make_pair(ip, url_infos));
    }
  } else if (it->second.size() < kMaxUrlsPerIP) {
    it->second.push_back(IPUrlInfo(url, method, referrer, resource_type));
  }
}

void ClientSideDetectionHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(type, content::NOTIFICATION_RESOURCE_RESPONSE_STARTED);
  const ResourceRequestDetails* req = content::Details<ResourceRequestDetails>(
      details).ptr();
  if (req && browse_info_.get() &&
      should_extract_malware_features_ && req->url.is_valid()) {
    UpdateIPUrlMap(req->socket_address.host() /* ip */,
                   req->url.spec()  /* url */,
                   req->method,
                   req->referrer,
                   req->resource_type);
  }
}

bool ClientSideDetectionHost::DidShowSBInterstitial() const {
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
  if (ui_manager_.get())
    ui_manager_->RemoveObserver(this);

  ui_manager_ = ui_manager;
  if (ui_manager)
    ui_manager_->AddObserver(this);

  database_manager_ = database_manager;
}

}  // namespace safe_browsing
