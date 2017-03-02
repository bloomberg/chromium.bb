// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_CONTENT_SUGGESTIONS_NOTIFICATION_HELPER_H_
#define CHROME_BROWSER_ANDROID_NTP_CONTENT_SUGGESTIONS_NOTIFICATION_HELPER_H_

#include <jni.h>
#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/ntp_snippets/ntp_snippets_metrics.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "url/gurl.h"

class Profile;

namespace gfx {
class Image;
}  // namespace gfx

namespace ntp_snippets {

class ContentSuggestionsNotificationHelper {
 public:
  static bool SendNotification(const ContentSuggestion::ID& id,
                               const GURL& url,
                               const base::string16& title,
                               const base::string16& text,
                               const gfx::Image& image,
                               base::Time timeout_at,
                               int priority);
  static void HideNotification(const ContentSuggestion::ID& id,
                               ContentSuggestionsNotificationAction why);
  static void HideAllNotifications(ContentSuggestionsNotificationAction why);

  // Moves metrics tracked in Java into native histograms. Should be called when
  // the native library starts up, to capture any actions that were taken since
  // the last time it was running. (Harmless to call more often, though)
  //
  // Also updates the "consecutive ignored" preference, which is computed from
  // the actions taken on notifications, and maybe the "opt outs" metric, which
  // is computed in turn from that.
  static void FlushCachedMetrics();

  // True if the user has ignored enough notifications that we no longer think
  // that the user is interested in them.
  static bool IsDisabledForProfile(Profile* profile);

  // Registers JNI methods.
  static bool Register(JNIEnv* env);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSuggestionsNotificationHelper);
};

}  // namespace ntp_snippets

#endif  // CHROME_BROWSER_ANDROID_NTP_CONTENT_SUGGESTIONS_NOTIFICATION_HELPER_H_
