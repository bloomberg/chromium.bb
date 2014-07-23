// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

#if defined(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(safe_browsing::SafeBrowsingTabObserver);

namespace safe_browsing {

#if !defined(FULL_SAFE_BROWSING)
// Provide a dummy implementation so that scoped_ptr<ClientSideDetectionHost>
// has a concrete destructor to call. This is necessary because it is used
// as a member of SafeBrowsingTabObserver, even if it only ever contains NULL.
class ClientSideDetectionHost { };
#endif

SafeBrowsingTabObserver::SafeBrowsingTabObserver(
    content::WebContents* web_contents) : web_contents_(web_contents) {
#if defined(FULL_SAFE_BROWSING)
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  if (prefs) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        prefs::kSafeBrowsingEnabled,
        base::Bind(&SafeBrowsingTabObserver::UpdateSafebrowsingDetectionHost,
                   base::Unretained(this)));

    if (prefs->GetBoolean(prefs::kSafeBrowsingEnabled) &&
        g_browser_process->safe_browsing_detection_service()) {
      safebrowsing_detection_host_.reset(
          ClientSideDetectionHost::Create(web_contents));
    }
  }
#endif
}

SafeBrowsingTabObserver::~SafeBrowsingTabObserver() {
}

////////////////////////////////////////////////////////////////////////////////
// Internal helpers

void SafeBrowsingTabObserver::UpdateSafebrowsingDetectionHost() {
#if defined(FULL_SAFE_BROWSING)
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  bool safe_browsing = prefs->GetBoolean(prefs::kSafeBrowsingEnabled);
  if (safe_browsing &&
      g_browser_process->safe_browsing_detection_service()) {
    if (!safebrowsing_detection_host_.get()) {
      safebrowsing_detection_host_.reset(
          ClientSideDetectionHost::Create(web_contents_));
    }
  } else {
    safebrowsing_detection_host_.reset();
  }

  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  rvh->Send(new ChromeViewMsg_SetClientSidePhishingDetection(
      rvh->GetRoutingID(), safe_browsing));
#endif
}

  // Forwards to detection host is client-side detection is enabled.
bool SafeBrowsingTabObserver::DidPageReceiveSafeBrowsingMatch() const {
#if defined(FULL_SAFE_BROWSING)
  return safebrowsing_detection_host_ &&
      safebrowsing_detection_host_->DidPageReceiveSafeBrowsingMatch();
#else
  return false;
#endif
}

}  // namespace safe_browsing
