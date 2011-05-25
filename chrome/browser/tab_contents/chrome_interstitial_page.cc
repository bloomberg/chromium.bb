// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/chrome_interstitial_page.h"

#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"

ChromeInterstitialPage::ChromeInterstitialPage(TabContents* tab,
                                               bool new_navigation,
                                               const GURL& url)
    : InterstitialPage(tab, new_navigation, url) {
  RendererPreferences prefs;
  renderer_preferences_util::UpdateFromSystemSettings(
      &prefs, tab->profile());
  set_renderer_preferences(prefs);
}

ChromeInterstitialPage::~ChromeInterstitialPage() {
}

void ChromeInterstitialPage::Show() {
  InterstitialPage::Show();

  notification_registrar_.Add(this, NotificationType::DOM_OPERATION_RESPONSE,
      Source<RenderViewHost>(render_view_host()));
}

void ChromeInterstitialPage::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  if (NotificationType::DOM_OPERATION_RESPONSE == type.value) {
    if (enabled()) {
      Details<DomOperationNotificationDetails> dom_op_details(details);
      CommandReceived(dom_op_details->json());
    }
    return;
  }
  InterstitialPage::Observe(type, source, details);
}
