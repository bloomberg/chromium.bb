// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/navigation_correction_tab_observer.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"

using content::RenderFrameHost;
using content::RenderViewHost;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(NavigationCorrectionTabObserver);

NavigationCorrectionTabObserver::NavigationCorrectionTabObserver(
    WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
  PrefService* prefs = profile_->GetPrefs();
  if (prefs) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        prefs::kAlternateErrorPagesEnabled,
        base::Bind(&NavigationCorrectionTabObserver::OnEnabledChanged,
                   base::Unretained(this)));
  }

  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
}

NavigationCorrectionTabObserver::~NavigationCorrectionTabObserver() {
}

// static
void NavigationCorrectionTabObserver::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kAlternateErrorPagesEnabled,
                             true,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsObserver overrides

void NavigationCorrectionTabObserver::RenderViewCreated(
    RenderViewHost* render_view_host) {
  UpdateNavigationCorrectionInfo(render_view_host);
}

////////////////////////////////////////////////////////////////////////////////
// content::NotificationObserver overrides

void NavigationCorrectionTabObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_GOOGLE_URL_UPDATED, type);
  UpdateNavigationCorrectionInfo(web_contents()->GetRenderViewHost());
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

GURL NavigationCorrectionTabObserver::GetNavigationCorrectionURL() const {
  // Disable navigation corrections when the preference is disabled or when in
  // Incognito mode.
  if (!profile_->GetPrefs()->GetBoolean(prefs::kAlternateErrorPagesEnabled) ||
      profile_->IsOffTheRecord()) {
    return GURL();
  }

  return google_util::LinkDoctorBaseURL();
}

void NavigationCorrectionTabObserver::OnEnabledChanged() {
  UpdateNavigationCorrectionInfo(web_contents()->GetRenderViewHost());
}

void NavigationCorrectionTabObserver::UpdateNavigationCorrectionInfo(
    RenderViewHost* rvh) {
  RenderFrameHost* rfh = rvh->GetMainFrame();
  rfh->Send(new ChromeViewMsg_SetNavigationCorrectionInfo(
      rfh->GetRoutingID(), GetNavigationCorrectionURL(),
      google_util::GetGoogleLocale(),
      google_util::GetGoogleCountryCode(profile_), google_apis::GetAPIKey(),
      google_util::GetGoogleSearchURL(profile_)));
}
