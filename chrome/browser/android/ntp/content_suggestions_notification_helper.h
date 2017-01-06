// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_CONTENT_SUGGESTIONS_NOTIFICATION_HELPER_H_
#define CHROME_BROWSER_ANDROID_NTP_CONTENT_SUGGESTIONS_NOTIFICATION_HELPER_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace ntp_snippets {

class ContentSuggestionsNotificationHelper {
 public:
  static void OpenURL(const GURL& url);
  static void SendNotification(const GURL& url,
                               const base::string16& title,
                               const base::string16& text,
                               const gfx::Image& image,
                               base::Time timeout_at);
  static void HideAllNotifications();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSuggestionsNotificationHelper);
};

}  // namespace ntp_snippets

#endif  // CHROME_BROWSER_ANDROID_NTP_CONTENT_SUGGESTIONS_NOTIFICATION_HELPER_H_
