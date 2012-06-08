// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/client_side_detection_host.h"
#endif

namespace safe_browsing {

#if !defined(ENABLE_SAFE_BROWSING)
// Provide a dummy implementation so that scoped_ptr<ClientSideDetectionHost>
// has a concrete destructor to call. This is necessary because it is used
// as a member of SafeBrowsingTabObserver, even if it only ever contains NULL.
class ClientSideDetectionHost { };
#endif

SafeBrowsingTabObserver::SafeBrowsingTabObserver(
    TabContents* tab_contents) : tab_contents_(tab_contents) {
#if defined(ENABLE_SAFE_BROWSING)
  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  if (prefs) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(prefs::kSafeBrowsingEnabled, this);

    if (prefs->GetBoolean(prefs::kSafeBrowsingEnabled) &&
        g_browser_process->safe_browsing_detection_service()) {
      safebrowsing_detection_host_.reset(
          ClientSideDetectionHost::Create(tab_contents_->web_contents()));
    }
  }
#endif
}

SafeBrowsingTabObserver::~SafeBrowsingTabObserver() {
}

////////////////////////////////////////////////////////////////////////////////
// content::NotificationObserver overrides

void SafeBrowsingTabObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name = content::Details<std::string>(details).ptr();
      DCHECK(content::Source<PrefService>(source).ptr() ==
             tab_contents_->profile()->GetPrefs());
      if (*pref_name == prefs::kSafeBrowsingEnabled) {
        UpdateSafebrowsingDetectionHost();
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

void SafeBrowsingTabObserver::UpdateSafebrowsingDetectionHost() {
#if defined(ENABLE_SAFE_BROWSING)
  PrefService* prefs = tab_contents_->profile()->GetPrefs();
  bool safe_browsing = prefs->GetBoolean(prefs::kSafeBrowsingEnabled);
  if (safe_browsing &&
      g_browser_process->safe_browsing_detection_service()) {
    if (!safebrowsing_detection_host_.get()) {
      safebrowsing_detection_host_.reset(
          ClientSideDetectionHost::Create(tab_contents_->web_contents()));
    }
  } else {
    safebrowsing_detection_host_.reset();
  }

  content::RenderViewHost* rvh =
      tab_contents_->web_contents()->GetRenderViewHost();
  rvh->Send(new ChromeViewMsg_SetClientSidePhishingDetection(
      rvh->GetRoutingID(), safe_browsing));
#endif
}

}  // namespace safe_browsing
