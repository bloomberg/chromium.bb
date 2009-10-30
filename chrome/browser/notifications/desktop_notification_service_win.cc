// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/common/child_process_host.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

// Creates a data:xxxx URL which contains the full HTML for a notification
// using supplied icon, title, and text, run through a template which contains
// the standard formatting for notifications.
static string16 CreateDataUrl(const GURL& icon_url, const string16& title,
    const string16& body) {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_NOTIFICATION_HTML));

  if (template_html.empty()) {
    NOTREACHED() << "unable to load template. ID: " << IDR_NOTIFICATION_HTML;
    return EmptyString16();
  }

  std::vector<string16> subst;
  if (icon_url.is_valid())
    subst.push_back(UTF8ToUTF16(icon_url.spec()));
  else
    subst.push_back(EmptyString16());

  subst.push_back(title);
  subst.push_back(body);

  if (icon_url.is_valid()) {
    subst.push_back(ASCIIToUTF16("block"));
    subst.push_back(ASCIIToUTF16("60"));
  } else {
    subst.push_back(ASCIIToUTF16("none"));
    subst.push_back(ASCIIToUTF16("5"));
  }

  string16 format_string = ASCIIToUTF16("data:text/html;charset=utf-8,"
                                        + template_html.as_string());
  return ReplaceStringPlaceholders(format_string, subst, NULL);
}

// This will call the notification manager on the UI thread to actually
// put the notification with the requested parameters on the desktop.
void DesktopNotificationService::ShowNotification(
    const Notification& notification) {
  ui_manager_->Add(notification, profile_);
}

// Shows a notification bubble which contains the contents of url.
bool DesktopNotificationService::ShowDesktopNotification(
    const GURL& origin, const GURL& url, int process_id, int route_id,
    NotificationSource source, int notification_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  NotificationObjectProxy* proxy =
      new NotificationObjectProxy(process_id, route_id,
                                  notification_id,
                                  source == WorkerNotification);
  Notification notif(origin, url, proxy);
  ShowNotification(notif);
  return true;
}

// Shows a notification constructed from an icon, title, and text.
bool DesktopNotificationService::ShowDesktopNotificationText(
    const GURL& origin, const GURL& icon, const string16& title,
    const string16& text, int process_id, int route_id,
    NotificationSource source, int notification_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  NotificationObjectProxy* proxy =
      new NotificationObjectProxy(process_id, route_id,
                                  notification_id,
                                  source == WorkerNotification);
  // "upconvert" the string parameters to a data: URL.
  string16 data_url = CreateDataUrl(icon, title, text);
  Notification notif(origin, GURL(data_url), proxy);
  ShowNotification(notif);
  return true;
}
