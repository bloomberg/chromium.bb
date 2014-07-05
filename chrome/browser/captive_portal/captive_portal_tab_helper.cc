// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"

#include "base/bind.h"
#include "chrome/browser/captive_portal/captive_portal_login_detector.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/captive_portal/captive_portal_tab_reloader.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_info.h"

using captive_portal::CaptivePortalResult;
using content::ResourceType;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(CaptivePortalTabHelper);

CaptivePortalTabHelper::CaptivePortalTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      // web_contents is NULL in unit tests.
      profile_(web_contents ? Profile::FromBrowserContext(
                                  web_contents->GetBrowserContext())
                            : NULL),
      tab_reloader_(
          new CaptivePortalTabReloader(
              profile_,
              web_contents,
              base::Bind(&CaptivePortalTabHelper::OpenLoginTab,
                         base::Unretained(this)))),
      login_detector_(new CaptivePortalLoginDetector(profile_)),
      web_contents_(web_contents),
      pending_error_code_(net::OK),
      provisional_render_view_host_(NULL) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
                 content::Source<content::WebContents>(web_contents));
}

CaptivePortalTabHelper::~CaptivePortalTabHelper() {
}

void CaptivePortalTabHelper::RenderViewDeleted(
    content::RenderViewHost* render_view_host) {
  // This can happen when a cross-process navigation is aborted, either by
  // pressing stop or by starting a new cross-process navigation that can't
  // re-use |provisional_render_view_host_|.  May also happen on a crash.
  if (render_view_host == provisional_render_view_host_)
    OnLoadAborted();
}

void CaptivePortalTabHelper::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  DCHECK(CalledOnValidThread());

  // Ignore subframes.
  if (render_frame_host->GetParent())
    return;

  content::RenderViewHost* render_view_host =
      render_frame_host->GetRenderViewHost();
  if (provisional_render_view_host_) {
    // If loading an error page for a previous failure, treat this as part of
    // the previous load.  Link Doctor pages act like two error page loads in a
    // row.  The second time, provisional_render_view_host_ will be NULL.
    if (is_error_page && provisional_render_view_host_ == render_view_host)
      return;
    // Otherwise, abort the old load.
    OnLoadAborted();
  }

  provisional_render_view_host_ = render_view_host;
  pending_error_code_ = net::OK;

  tab_reloader_->OnLoadStart(validated_url.SchemeIsSecure());
}

void CaptivePortalTabHelper::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    content::PageTransition transition_type) {
  DCHECK(CalledOnValidThread());

  // Ignore subframes.
  if (render_frame_host->GetParent())
    return;

  if (provisional_render_view_host_ == render_frame_host->GetRenderViewHost()) {
    tab_reloader_->OnLoadCommitted(pending_error_code_);
  } else {
    // This may happen if the active RenderView commits a page before a cross
    // process navigation cancels the old load.  In this case, the commit of the
    // old navigation will cancel the newer one.
    OnLoadAborted();

    // Send information about the new load.
    tab_reloader_->OnLoadStart(url.SchemeIsSecure());
    tab_reloader_->OnLoadCommitted(net::OK);
  }

  provisional_render_view_host_ = NULL;
  pending_error_code_ = net::OK;
}

void CaptivePortalTabHelper::DidFailProvisionalLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  DCHECK(CalledOnValidThread());

  // Ignore subframes and unexpected RenderViewHosts.
  if (render_frame_host->GetParent() ||
      render_frame_host->GetRenderViewHost() != provisional_render_view_host_)
    return;

  // Aborts generally aren't followed by loading an error page, so go ahead and
  // reset the state now, to prevent any captive portal checks from triggering.
  if (error_code == net::ERR_ABORTED) {
    OnLoadAborted();
    return;
  }

  pending_error_code_ = error_code;
}

void CaptivePortalTabHelper::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  DCHECK(CalledOnValidThread());

  login_detector_->OnStoppedLoading();
}

void CaptivePortalTabHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(CalledOnValidThread());
  switch (type) {
    case content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT: {
      DCHECK_EQ(web_contents(),
                content::Source<content::WebContents>(source).ptr());

      const content::ResourceRedirectDetails* redirect_details =
          content::Details<content::ResourceRedirectDetails>(details).ptr();

      OnRedirect(redirect_details->origin_child_id,
                 redirect_details->resource_type,
                 redirect_details->new_url);
      break;
    }
    case chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT: {
      DCHECK_EQ(profile_, content::Source<Profile>(source).ptr());

      const CaptivePortalService::Results* results =
          content::Details<CaptivePortalService::Results>(details).ptr();

      OnCaptivePortalResults(results->previous_result, results->result);
      break;
    }
    default:
      NOTREACHED();
  }
}

void CaptivePortalTabHelper::OnSSLCertError(const net::SSLInfo& ssl_info) {
  tab_reloader_->OnSSLCertError(ssl_info);
}

bool CaptivePortalTabHelper::IsLoginTab() const {
  return login_detector_->is_login_tab();
}

void CaptivePortalTabHelper::OnRedirect(int child_id,
                                        ResourceType::Type resource_type,
                                        const GURL& new_url) {
  // Only main frame redirects for the provisional RenderViewHost matter.
  if (resource_type != ResourceType::MAIN_FRAME ||
      !provisional_render_view_host_ ||
      provisional_render_view_host_->GetProcess()->GetID() != child_id) {
    return;
  }

  tab_reloader_->OnRedirect(new_url.SchemeIsSecure());
}

void CaptivePortalTabHelper::OnCaptivePortalResults(
    CaptivePortalResult previous_result,
    CaptivePortalResult result) {
  tab_reloader_->OnCaptivePortalResults(previous_result, result);
  login_detector_->OnCaptivePortalResults(previous_result, result);
}

void CaptivePortalTabHelper::OnLoadAborted() {
  // No further messages for the cancelled navigation will occur.
  provisional_render_view_host_ = NULL;
  // May have been aborting the load of an error page.
  pending_error_code_ = net::OK;

  tab_reloader_->OnAbort();
}

void CaptivePortalTabHelper::SetIsLoginTab() {
  login_detector_->SetIsLoginTab();
}

void CaptivePortalTabHelper::SetTabReloaderForTest(
    CaptivePortalTabReloader* tab_reloader) {
  tab_reloader_.reset(tab_reloader);
}

CaptivePortalTabReloader* CaptivePortalTabHelper::GetTabReloaderForTest() {
  return tab_reloader_.get();
}

void CaptivePortalTabHelper::OpenLoginTab() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);

  // If the Profile doesn't have a tabbed browser window open, do nothing.
  if (!browser)
    return;

  // Check if the Profile's topmost browser window already has a login tab.
  // If so, do nothing.
  // TODO(mmenke):  Consider focusing that tab, at least if this is the tab
  //                helper for the currently active tab for the profile.
  for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetWebContentsAt(i);
    CaptivePortalTabHelper* captive_portal_tab_helper =
        CaptivePortalTabHelper::FromWebContents(web_contents);
    if (captive_portal_tab_helper->IsLoginTab())
      return;
  }

  // Otherwise, open a login tab.  Only end up here when a captive portal result
  // was received, so it's safe to assume |profile_| has a CaptivePortalService.
  content::WebContents* web_contents = chrome::AddSelectedTabWithURL(
          browser,
          CaptivePortalServiceFactory::GetForProfile(profile_)->test_url(),
          content::PAGE_TRANSITION_TYPED);
  CaptivePortalTabHelper* captive_portal_tab_helper =
      CaptivePortalTabHelper::FromWebContents(web_contents);
  captive_portal_tab_helper->SetIsLoginTab();
}
