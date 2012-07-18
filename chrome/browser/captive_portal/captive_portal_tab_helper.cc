// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"

#include "base/bind.h"
#include "chrome/browser/captive_portal/captive_portal_login_detector.h"
#include "chrome/browser/captive_portal/captive_portal_tab_reloader.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_request_details.h"
#include "net/base/net_errors.h"

namespace captive_portal {

CaptivePortalTabHelper::CaptivePortalTabHelper(
    Profile* profile,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      tab_reloader_(
          new CaptivePortalTabReloader(
              profile,
              web_contents,
              base::Bind(&CaptivePortalTabHelper::OpenLoginTab,
                         base::Unretained(this)))),
      login_detector_(new CaptivePortalLoginDetector(profile)),
      profile_(profile),
      pending_error_code_(net::OK),
      provisional_main_frame_id_(-1) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
                 content::Source<content::WebContents>(web_contents));
}

CaptivePortalTabHelper::~CaptivePortalTabHelper() {
}

void CaptivePortalTabHelper::DidStartProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    bool is_error_page,
    content::RenderViewHost* render_view_host) {
  DCHECK(CalledOnValidThread());

  // Ignore subframes.
  if (!is_main_frame)
    return;

  provisional_main_frame_id_ = frame_id;

  // If loading an error page for a previous failure, treat this as part of
  // the previous load.  The second check is needed because Link Doctor pages
  // result in two error page provisional loads in a row.  Currently, the
  // second load is treated as a normal load, rather than reusing old error
  // codes.
  if (is_error_page && pending_error_code_ != net::OK)
    return;

  // Makes the second load for Link Doctor pages act as a normal load.
  // TODO(mmenke):  Figure out if this affects any other cases.
  pending_error_code_ = net::OK;

  tab_reloader_->OnLoadStart(validated_url.SchemeIsSecure());
}

void CaptivePortalTabHelper::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  DCHECK(CalledOnValidThread());

  // Ignore subframes.
  if (!is_main_frame)
    return;

  provisional_main_frame_id_ = -1;

  tab_reloader_->OnLoadCommitted(pending_error_code_);
  pending_error_code_ = net::OK;
}

void CaptivePortalTabHelper::DidFailProvisionalLoad(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    content::RenderViewHost* render_view_host) {
  DCHECK(CalledOnValidThread());

  // Ignore subframes.
  if (!is_main_frame)
    return;

  provisional_main_frame_id_ = -1;

  // Aborts generally aren't followed by loading an error page, so go ahead and
  // reset the state now, to prevent any captive portal checks from triggering.
  if (error_code == net::ERR_ABORTED) {
    // May have been aborting the load of an error page.
    pending_error_code_ = net::OK;

    tab_reloader_->OnAbort();
    return;
  }

  pending_error_code_ = error_code;
}

void CaptivePortalTabHelper::DidStopLoading() {
  DCHECK(CalledOnValidThread());

  login_detector_->OnStoppedLoading();
}

void CaptivePortalTabHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(CalledOnValidThread());
  if (type == content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT) {
    DCHECK_EQ(web_contents(),
              content::Source<content::WebContents>(source).ptr());

    const content::ResourceRedirectDetails* redirect_details =
        content::Details<content::ResourceRedirectDetails>(details).ptr();

    if (redirect_details->resource_type == ResourceType::MAIN_FRAME)
      OnRedirect(redirect_details->frame_id, redirect_details->new_url);
  } else if (type == chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT) {
    DCHECK_EQ(profile_, content::Source<Profile>(source).ptr());

    const CaptivePortalService::Results* results =
        content::Details<CaptivePortalService::Results>(details).ptr();

    OnCaptivePortalResults(results->previous_result, results->result);
  } else {
    NOTREACHED();
  }
}

void CaptivePortalTabHelper::OnRedirect(int64 frame_id, const GURL& new_url) {
  // If the main frame's not currently loading, or the redirect is for some
  // other frame, ignore the redirect.  It's unclear if |frame_id| can ever be
  // -1 ("invalid/unknown"), but best to be careful.
  if (provisional_main_frame_id_ == -1 ||
      provisional_main_frame_id_ != frame_id) {
    return;
  }

  tab_reloader_->OnRedirect(new_url.SchemeIsSecure());
}

void CaptivePortalTabHelper::OnCaptivePortalResults(Result previous_result,
                                                    Result result) {
  tab_reloader_->OnCaptivePortalResults(previous_result, result);
  login_detector_->OnCaptivePortalResults(previous_result, result);
}

bool CaptivePortalTabHelper::IsLoginTab() const {
  return login_detector_->is_login_tab();
}

void CaptivePortalTabHelper::SetIsLoginTab() {
  login_detector_->set_is_login_tab();
}

void CaptivePortalTabHelper::SetTabReloaderForTest(
    CaptivePortalTabReloader* tab_reloader) {
  tab_reloader_.reset(tab_reloader);
}

CaptivePortalTabReloader* CaptivePortalTabHelper::GetTabReloaderForTest() {
  return tab_reloader_.get();
}

void CaptivePortalTabHelper::OpenLoginTab() {
  Browser* browser = browser::FindTabbedBrowser(profile_, true);
  // If the Profile doesn't have a tabbed browser window open, do nothing.
  if (!browser)
    return;

  // Check if the Profile's topmost browser window already has a login tab.
  // If so, do nothing.
  // TODO(mmenke):  Consider focusing that tab, at least if this is the tab
  //                helper for the currently active tab for the profile.
  for (int i = 0; i < browser->tab_count(); ++i) {
    TabContents* tab_contents = chrome::GetTabContentsAt(browser, i);
    if (tab_contents->captive_portal_tab_helper()->IsLoginTab())
      return;
  }

  // Otherwise, open a login tab.  Only end up here when a captive portal result
  // was received, so it's safe to assume |profile_| has a CaptivePortalService.
  TabContents* tab_contents = chrome::AddSelectedTabWithURL(
          browser,
          CaptivePortalServiceFactory::GetForProfile(profile_)->test_url(),
          content::PAGE_TRANSITION_TYPED);
  tab_contents->captive_portal_tab_helper()->SetIsLoginTab();
}

}  // namespace captive_portal
