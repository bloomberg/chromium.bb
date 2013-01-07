// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/alternate_error_tab_observer.h"

#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::RenderViewHost;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(AlternateErrorPageTabObserver);

AlternateErrorPageTabObserver::AlternateErrorPageTabObserver(
    WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
  PrefServiceSyncable* prefs = profile_->GetPrefs();
  if (prefs) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        prefs::kAlternateErrorPagesEnabled,
        base::Bind(&AlternateErrorPageTabObserver::
                       OnAlternateErrorPagesEnabledChanged,
                   base::Unretained(this)));
  }

  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
}

AlternateErrorPageTabObserver::~AlternateErrorPageTabObserver() {
}

// static
void AlternateErrorPageTabObserver::RegisterUserPrefs(
    PrefServiceSyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kAlternateErrorPagesEnabled, true,
                             PrefServiceSyncable::SYNCABLE_PREF);
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsObserver overrides

void AlternateErrorPageTabObserver::RenderViewCreated(
    RenderViewHost* render_view_host) {
  UpdateAlternateErrorPageURL(render_view_host);
}

////////////////////////////////////////////////////////////////////////////////
// content::NotificationObserver overrides

void AlternateErrorPageTabObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_GOOGLE_URL_UPDATED, type);
  UpdateAlternateErrorPageURL(web_contents()->GetRenderViewHost());
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

GURL AlternateErrorPageTabObserver::GetAlternateErrorPageURL() const {
  GURL url;
  // Disable alternate error pages when in Incognito mode.
  if (profile_->IsOffTheRecord())
    return url;

  if (profile_->GetPrefs()->GetBoolean(prefs::kAlternateErrorPagesEnabled)) {
    url = google_util::LinkDoctorBaseURL();
    if (!url.is_valid())
      return url;
    url = google_util::AppendGoogleLocaleParam(url);
    url = google_util::AppendGoogleTLDParam(profile_, url);
  }
  return url;
}

void AlternateErrorPageTabObserver::OnAlternateErrorPagesEnabledChanged() {
  UpdateAlternateErrorPageURL(web_contents()->GetRenderViewHost());
}

void AlternateErrorPageTabObserver::UpdateAlternateErrorPageURL(
    RenderViewHost* rvh) {
  rvh->SetAltErrorPageURL(GetAlternateErrorPageURL());
}
