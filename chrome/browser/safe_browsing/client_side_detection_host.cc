// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_host.h"

#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/safebrowsing_messages.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "googleurl/src/gurl.h"

namespace safe_browsing {

// This class is instantiated each time a new toplevel URL loads, and
// asynchronously checks whether the phishing classifier should run for this
// URL.  If so, it notifies the renderer with a StartPhishingDetection IPC.
class ClientSideDetectionHost::ShouldClassifyUrlRequest
    : public NotificationObserver {
 public:
  ShouldClassifyUrlRequest(const ViewHostMsg_FrameNavigate_Params& params,
                           TabContents* tab_contents,
                           ClientSideDetectionService* service)
      : params_(params),
        tab_contents_(tab_contents),
        service_(service),
        ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(tab_contents_);
    DCHECK(service_);
    registrar_.Add(this,
                   NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab_contents));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::TAB_CONTENTS_DESTROYED:
        Finish(false);
        break;
      default:
        NOTREACHED();
    };
  }

  void Start() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // For consistency, always run the pre-classification checks
    // asynchronously.
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            method_factory_.NewRunnableMethod(
                                &ShouldClassifyUrlRequest::Run));
  }

 private:
  // This object always deletes itself, so make the destructor private.
  virtual ~ShouldClassifyUrlRequest() {}

  void Run() {
    // Don't run the phishing classifier if the URL came from a private
    // network, since we don't want to ping back in this case.  We also need
    // to check whether the connection was proxied -- if so, we won't have the
    // correct remote IP address, and will skip phishing classification.
    if (params_.was_fetched_via_proxy) {
      VLOG(1) << "Skipping phishing classification for URL: " << params_.url
              << " because it was fetched via a proxy.";
      UMA_HISTOGRAM_COUNTS("SBClientPhishing.NoClassifyProxyFetch", 1);
      Finish(false);
      return;
    }
    if (service_->IsPrivateIPAddress(params_.socket_address.host())) {
      VLOG(1) << "Skipping phishing classification for URL: " << params_.url
              << " because of hosting on private IP: "
              << params_.socket_address.host();
      UMA_HISTOGRAM_COUNTS("SBClientPhishing.NoClassifyPrivateIP", 1);
      Finish(false);
      return;
    }

    Finish(true);
  }

  void Finish(bool should_classify) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (should_classify) {
      RenderViewHost* rvh = tab_contents_->render_view_host();
      rvh->Send(new ViewMsg_StartPhishingDetection(
          rvh->routing_id(), params_.url));
    }
    delete this;
  }

  ViewHostMsg_FrameNavigate_Params params_;
  TabContents* tab_contents_;
  ClientSideDetectionService* service_;
  NotificationRegistrar registrar_;
  ScopedRunnableMethodFactory<ShouldClassifyUrlRequest> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShouldClassifyUrlRequest);
};

// This class is used to display the phishing interstitial.
class CsdClient : public SafeBrowsingService::Client {
 public:
  CsdClient() {}

  // Method from SafeBrowsingService::Client.  This method is called on the
  // IO thread once the interstitial is going away.  This method simply deletes
  // the CsdClient object.
  virtual void OnBlockingPageComplete(bool proceed) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    // Delete this on the UI thread since it was created there.
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            new DeleteTask<CsdClient>(this));
  }

 private:
  friend class DeleteTask<CsdClient>;  // Calls the private destructor.

  // We're taking care of deleting this object.  No-one else should delete
  // this object.
  virtual ~CsdClient() {}

  DISALLOW_COPY_AND_ASSIGN(CsdClient);
};

ClientSideDetectionHost::ClientSideDetectionHost(TabContents* tab)
    : TabContentsObserver(tab),
      service_(g_browser_process->safe_browsing_detection_service()),
      cb_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(tab);
  // Note: service_ and sb_service_ might be NULL.
  ResourceDispatcherHost* resource =
      g_browser_process->resource_dispatcher_host();
  if (resource) {
    sb_service_ = resource->safe_browsing_service();
  }
}

ClientSideDetectionHost::~ClientSideDetectionHost() {
}

bool ClientSideDetectionHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ClientSideDetectionHost, message)
    IPC_MESSAGE_HANDLER(SafeBrowsingDetectionHostMsg_DetectedPhishingSite,
                        OnDetectedPhishingSite)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ClientSideDetectionHost::DidNavigateMainFramePostCommit(
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  // TODO(noelutz): move this DCHECK to TabContents and fix all the unit tests
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
  cb_factory_.RevokeAll();

  if (service_) {
    // Notify the renderer if it should classify this URL.
    ShouldClassifyUrlRequest* request =
        new ShouldClassifyUrlRequest(params, tab_contents(), service_);
    request->Start();
  }
}

void ClientSideDetectionHost::OnDetectedPhishingSite(const GURL& phishing_url,
                                                     double phishing_score) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // There is something seriously wrong if there is no service class but
  // this method is called.  The renderer should not start phishing detection
  // if there isn't any service class in the browser.
  DCHECK(service_);
  if (service_) {
    // There shouldn't be any pending requests because we revoke them everytime
    // we navigate away.
    DCHECK(!cb_factory_.HasPendingCallbacks());
    service_->SendClientReportPhishingRequest(
        phishing_url,
        phishing_score,
        cb_factory_.NewCallback(
            &ClientSideDetectionHost::MaybeShowPhishingWarning));
  }
}

void ClientSideDetectionHost::MaybeShowPhishingWarning(GURL phishing_url,
                                                       bool is_phishing) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (is_phishing &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableClientSidePhishingInterstitial)) {
    DCHECK(tab_contents());
    // TODO(noelutz): this is not perfect.  It's still possible that the
    // user browses away before the interstitial is shown.  Maybe we should
    // stop all pending navigations?
    if (sb_service_) {
      // TODO(noelutz): refactor the SafeBrowsing service class and the
      // SafeBrowsing blocking page class so that we don't need to depend
      // on the SafeBrowsingService here and so that we don't need to go
      // through the IO message loop.
      std::vector<GURL> redirect_urls;
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          NewRunnableMethod(sb_service_.get(),
                            &SafeBrowsingService::DisplayBlockingPage,
                            phishing_url, phishing_url,
                            redirect_urls,
                            // We only classify the main frame URL.
                            ResourceType::MAIN_FRAME,
                            // TODO(noelutz): create a separate threat type
                            // for client-side phishing detection.
                            SafeBrowsingService::URL_PHISHING,
                            new CsdClient() /* will delete itself */,
                            tab_contents()->GetRenderProcessHost()->id(),
                            tab_contents()->render_view_host()->routing_id()));
    }
  }
}

void ClientSideDetectionHost::set_client_side_detection_service(
    ClientSideDetectionService* service) {
  service_ = service;
}

void ClientSideDetectionHost::set_safe_browsing_service(
    SafeBrowsingService* service) {
  sb_service_ = service;
}

}  // namespace safe_browsing
