// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/alternate_error_tab_observer.h"

#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"

AlternateErrorPageTabObserver::AlternateErrorPageTabObserver(
    TabContentsWrapper* wrapper)
        : TabContentsObserver(wrapper->tab_contents()),
          wrapper_(wrapper) {
  PrefService* prefs = wrapper_->profile()->GetPrefs();
  if (prefs) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(prefs::kAlternateErrorPagesEnabled, this);
  }

  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
                 content::NotificationService::AllSources());
}

AlternateErrorPageTabObserver::~AlternateErrorPageTabObserver() {
}

// static
void AlternateErrorPageTabObserver::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kAlternateErrorPagesEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsObserver overrides

void AlternateErrorPageTabObserver::RenderViewCreated(
    RenderViewHost* render_view_host) {
  UpdateAlternateErrorPageURL(render_view_host);
}

////////////////////////////////////////////////////////////////////////////////
// content::NotificationObserver overrides

void AlternateErrorPageTabObserver::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_URL_UPDATED:
      UpdateAlternateErrorPageURL(tab_contents()->GetRenderViewHost());
      break;
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name = content::Details<std::string>(details).ptr();
      DCHECK(content::Source<PrefService>(source).ptr() ==
             wrapper_->profile()->GetPrefs());
      if (*pref_name == prefs::kAlternateErrorPagesEnabled) {
        UpdateAlternateErrorPageURL(tab_contents()->GetRenderViewHost());
      } else {
        NOTREACHED() << "unexpected pref change notification" << *pref_name;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

GURL AlternateErrorPageTabObserver::GetAlternateErrorPageURL() const {
  GURL url;
  // Disable alternate error pages when in Incognito mode.
  if (wrapper_->profile()->IsOffTheRecord())
    return url;

  PrefService* prefs = wrapper_->profile()->GetPrefs();
  if (prefs->GetBoolean(prefs::kAlternateErrorPagesEnabled)) {
    url = google_util::AppendGoogleLocaleParam(
        GURL(google_util::kLinkDoctorBaseURL));
    url = google_util::AppendGoogleTLDParam(url);
  }
  return url;
}

void AlternateErrorPageTabObserver::UpdateAlternateErrorPageURL(
    RenderViewHost* rvh) {
  rvh->SetAltErrorPageURL(GetAlternateErrorPageURL());
}
