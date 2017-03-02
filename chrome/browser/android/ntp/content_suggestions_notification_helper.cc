// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/content_suggestions_notification_helper.h"

#include <limits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ntp_snippets/ntp_snippets_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "jni/ContentSuggestionsNotificationHelper_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

using base::android::JavaParamRef;
using params::ntp_snippets::kNotificationsFeature;
using params::ntp_snippets::kNotificationsIgnoredLimitParam;
using params::ntp_snippets::kNotificationsIgnoredDefaultLimit;

namespace ntp_snippets {

namespace {

bool IsDisabledForProfile(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  int current =
      prefs->GetInteger(prefs::kContentSuggestionsConsecutiveIgnoredPrefName);
  int limit = variations::GetVariationParamByFeatureAsInt(
      kNotificationsFeature, kNotificationsIgnoredLimitParam,
      kNotificationsIgnoredDefaultLimit);
  return current >= limit;
}

}  // namespace

bool ContentSuggestionsNotificationHelper::SendNotification(
    const ContentSuggestion::ID& id,
    const GURL& url,
    const base::string16& title,
    const base::string16& text,
    const gfx::Image& image,
    base::Time timeout_at,
    int priority) {
  JNIEnv* env = base::android::AttachCurrentThread();
  SkBitmap skimage = image.AsImageSkia().GetRepresentation(1.0f).sk_bitmap();
  if (skimage.empty())
    return false;

  jlong timeout_at_millis = timeout_at.ToJavaTime();
  if (timeout_at == base::Time::Max()) {
    timeout_at_millis = std::numeric_limits<jlong>::max();
  }

  if (Java_ContentSuggestionsNotificationHelper_showNotification(
          env, id.category().id(),
          base::android::ConvertUTF8ToJavaString(env, id.id_within_category()),
          base::android::ConvertUTF8ToJavaString(env, url.spec()),
          base::android::ConvertUTF16ToJavaString(env, title),
          base::android::ConvertUTF16ToJavaString(env, text),
          gfx::ConvertToJavaBitmap(&skimage), timeout_at_millis, priority)) {
    DVLOG(1) << "Displayed notification for " << id;
    return true;
  } else {
    DVLOG(1) << "Suppressed notification for " << url.spec();
    return false;
  }
}

void ContentSuggestionsNotificationHelper::HideNotification(
    const ContentSuggestion::ID& id,
    ContentSuggestionsNotificationAction why) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContentSuggestionsNotificationHelper_hideNotification(
      env, id.category().id(),
      base::android::ConvertUTF8ToJavaString(env, id.id_within_category()),
      static_cast<int>(why));
}

void ContentSuggestionsNotificationHelper::HideAllNotifications(
    ContentSuggestionsNotificationAction why) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContentSuggestionsNotificationHelper_hideAllNotifications(env, why);
}

void ContentSuggestionsNotificationHelper::FlushCachedMetrics() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContentSuggestionsNotificationHelper_flushCachedMetrics(env);
}

bool ContentSuggestionsNotificationHelper::IsDisabledForProfile(
    Profile* profile) {
  return ntp_snippets::IsDisabledForProfile(profile);
}

// static
bool ContentSuggestionsNotificationHelper::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void ReceiveFlushedMetrics(JNIEnv* env,
                                  const JavaParamRef<jclass>& class_object,
                                  jint tap_count,
                                  jint dismissal_count,
                                  jint hide_deadline_count,
                                  jint hide_expiry_count,
                                  jint hide_frontmost_count,
                                  jint hide_disabled_count,
                                  jint hide_shutdown_count,
                                  jint consecutive_ignored) {
  DVLOG(1) << "Flushing metrics: tap_count=" << tap_count
           << "; dismissal_count=" << dismissal_count
           << "; hide_deadline_count=" << hide_deadline_count
           << "; hide_expiry_count=" << hide_expiry_count
           << "; hide_frontmost_count=" << hide_frontmost_count
           << "; hide_disabled_count=" << hide_disabled_count
           << "; hide_shutdown_count=" << hide_shutdown_count
           << "; consecutive_ignored=" << consecutive_ignored;
  Profile* profile = ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
  PrefService* prefs = profile->GetPrefs();

  for (int i = 0; i < tap_count; ++i) {
    RecordContentSuggestionsNotificationAction(CONTENT_SUGGESTIONS_TAP);
  }
  for (int i = 0; i < dismissal_count; ++i) {
    RecordContentSuggestionsNotificationAction(CONTENT_SUGGESTIONS_DISMISSAL);
  }
  for (int i = 0; i < hide_deadline_count; ++i) {
    RecordContentSuggestionsNotificationAction(
        CONTENT_SUGGESTIONS_HIDE_DEADLINE);
  }
  for (int i = 0; i < hide_expiry_count; ++i) {
    RecordContentSuggestionsNotificationAction(CONTENT_SUGGESTIONS_HIDE_EXPIRY);
  }
  for (int i = 0; i < hide_frontmost_count; ++i) {
    RecordContentSuggestionsNotificationAction(
        CONTENT_SUGGESTIONS_HIDE_FRONTMOST);
  }
  for (int i = 0; i < hide_disabled_count; ++i) {
    RecordContentSuggestionsNotificationAction(
        CONTENT_SUGGESTIONS_HIDE_DISABLED);
  }
  for (int i = 0; i < hide_shutdown_count; ++i) {
    RecordContentSuggestionsNotificationAction(
        CONTENT_SUGGESTIONS_HIDE_SHUTDOWN);
  }

  const bool was_disabled = IsDisabledForProfile(profile);
  if (tap_count == 0) {
    // There were no taps, consecutive_ignored has not been reset and continues
    // from where it left off. If there was a tap, then Java has provided us
    // with the number of ignored notifications since that point.
    consecutive_ignored +=
        prefs->GetInteger(prefs::kContentSuggestionsConsecutiveIgnoredPrefName);
  }
  prefs->SetInteger(prefs::kContentSuggestionsConsecutiveIgnoredPrefName,
                    consecutive_ignored);
  const bool is_disabled = IsDisabledForProfile(profile);
  if (!was_disabled && is_disabled) {
    RecordContentSuggestionsNotificationOptOut(CONTENT_SUGGESTIONS_IMPLICIT);
  }
}

}  // namespace ntp_snippets
