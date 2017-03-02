// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/content_suggestions_notifier_service.h"

#include <algorithm>

#include "base/android/application_status_listener.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/ntp/content_suggestions_notification_helper.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ntp_snippets/ntp_snippets_features.h"
#include "chrome/browser/ntp_snippets/ntp_snippets_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

using ntp_snippets::Category;
using ntp_snippets::CategoryStatus;
using ntp_snippets::ContentSuggestion;
using ntp_snippets::ContentSuggestionsNotificationHelper;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::KnownCategories;
using params::ntp_snippets::kNotificationsAlwaysNotifyParam;
using params::ntp_snippets::kNotificationsDailyLimit;
using params::ntp_snippets::kNotificationsDefaultDailyLimit;
using params::ntp_snippets::kNotificationsDefaultPriority;
using params::ntp_snippets::kNotificationsFeature;
using params::ntp_snippets::kNotificationsKeepWhenFrontmostParam;
using params::ntp_snippets::kNotificationsOpenToNTPParam;
using params::ntp_snippets::kNotificationsPriorityParam;
using params::ntp_snippets::kNotificationsUseSnippetAsTextParam;

namespace {

const char kNotificationIDWithinCategory[] =
    "ContentSuggestionsNotificationIDWithinCategory";

gfx::Image CropSquare(const gfx::Image& image) {
  if (image.IsEmpty()) {
    return image;
  }
  const gfx::ImageSkia* skimage = image.ToImageSkia();
  gfx::Rect bounds{{0, 0}, skimage->size()};
  int size = std::min(bounds.width(), bounds.height());
  bounds.ClampToCenteredSize({size, size});
  return gfx::Image(gfx::ImageSkiaOperations::CreateTiledImage(
      *skimage, bounds.x(), bounds.y(), bounds.width(), bounds.height()));
}

bool ShouldNotifyInState(base::android::ApplicationState state) {
  switch (state) {
    case base::android::APPLICATION_STATE_UNKNOWN:
    case base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
      return false;
    case base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
      return true;
  }
  NOTREACHED();
  return false;
}

int DayAsYYYYMMDD() {
  base::Time::Exploded now{};
  base::Time::Now().LocalExplode(&now);
  return (now.year * 10000) + (now.month * 100) + now.day_of_month;
}

bool HaveQuotaForToday(PrefService* prefs) {
  int today = DayAsYYYYMMDD();
  int limit = variations::GetVariationParamByFeatureAsInt(
      kNotificationsFeature, kNotificationsDailyLimit,
      kNotificationsDefaultDailyLimit);
  int sent =
      prefs->GetInteger(prefs::kContentSuggestionsNotificationsSentDay) == today
          ? prefs->GetInteger(prefs::kContentSuggestionsNotificationsSentCount)
          : 0;
  return sent < limit;
}

void ConsumeQuota(PrefService* prefs) {
  int sent =
      prefs->GetInteger(prefs::kContentSuggestionsNotificationsSentCount);
  int today = DayAsYYYYMMDD();
  if (prefs->GetInteger(prefs::kContentSuggestionsNotificationsSentDay) !=
      today) {
    prefs->SetInteger(prefs::kContentSuggestionsNotificationsSentDay, today);
    sent = 0;  // Reset on day change.
  }
  prefs->SetInteger(prefs::kContentSuggestionsNotificationsSentCount, sent + 1);
}

}  // namespace

class ContentSuggestionsNotifierService::NotifyingObserver
    : public ContentSuggestionsService::Observer {
 public:
  NotifyingObserver(ContentSuggestionsService* service,
                    Profile* profile)
      : service_(service),
        profile_(profile),
        app_status_listener_(base::Bind(&NotifyingObserver::AppStatusChanged,
                                        base::Unretained(this))),
        weak_ptr_factory_(this) {}

  void OnNewSuggestions(Category category) override {
    if (!ShouldNotifyInState(app_status_listener_.GetState())) {
      DVLOG(1) << "Suppressed notification because Chrome is frontmost";
      return;
    } else if (ContentSuggestionsNotificationHelper::IsDisabledForProfile(
                   profile_)) {
      DVLOG(1) << "Suppressed notification due to opt-out";
      return;
    } else if (!HaveQuotaForToday(profile_->GetPrefs())) {
      DVLOG(1) << "Notification suppressed due to daily limit";
      return;
    }
    const ContentSuggestion* suggestion = GetSuggestionToNotifyAbout(category);
    if (!suggestion) {
      return;
    }
    base::Time timeout_at = suggestion->notification_extra()
                                ? suggestion->notification_extra()->deadline
                                : base::Time::Max();
    bool use_snippet = variations::GetVariationParamByFeatureAsBool(
        kNotificationsFeature, kNotificationsUseSnippetAsTextParam, false);
    bool open_to_ntp = variations::GetVariationParamByFeatureAsBool(
        kNotificationsFeature, kNotificationsOpenToNTPParam, false);
    service_->FetchSuggestionImage(
        suggestion->id(),
        base::Bind(&NotifyingObserver::ImageFetched,
                   weak_ptr_factory_.GetWeakPtr(), suggestion->id(),
                   open_to_ntp ? GURL("chrome://newtab") : suggestion->url(),
                   suggestion->title(),
                   use_snippet ? suggestion->snippet_text()
                               : suggestion->publisher_name(),
                   timeout_at));
  }

  void OnCategoryStatusChanged(Category category,
                               CategoryStatus new_status) override {
    if (!category.IsKnownCategory(KnownCategories::ARTICLES)) {
      return;
    }
    switch (new_status) {
      case CategoryStatus::AVAILABLE:
      case CategoryStatus::AVAILABLE_LOADING:
        break;  // nothing to do
      case CategoryStatus::INITIALIZING:
      case CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED:
      case CategoryStatus::CATEGORY_EXPLICITLY_DISABLED:
      case CategoryStatus::LOADING_ERROR:
      case CategoryStatus::NOT_PROVIDED:
      case CategoryStatus::SIGNED_OUT:
        ContentSuggestionsNotificationHelper::HideAllNotifications(
            CONTENT_SUGGESTIONS_HIDE_DISABLED);
        break;
    }
  }

  void OnSuggestionInvalidated(
      const ContentSuggestion::ID& suggestion_id) override {
    ContentSuggestionsNotificationHelper::HideNotification(
        suggestion_id, CONTENT_SUGGESTIONS_HIDE_EXPIRY);
  }

  void OnFullRefreshRequired() override {
    ContentSuggestionsNotificationHelper::HideAllNotifications(
        CONTENT_SUGGESTIONS_HIDE_EXPIRY);
  }

  void ContentSuggestionsServiceShutdown() override {
    ContentSuggestionsNotificationHelper::HideAllNotifications(
        CONTENT_SUGGESTIONS_HIDE_SHUTDOWN);
  }

 private:
  const ContentSuggestion* GetSuggestionToNotifyAbout(Category category) {
    const auto& suggestions = service_->GetSuggestionsForCategory(category);
    // TODO(sfiera): replace with AlwaysNotifyAboutContentSuggestions().
    if (variations::GetVariationParamByFeatureAsBool(
            kNotificationsFeature, kNotificationsAlwaysNotifyParam, false)) {
      if (category.IsKnownCategory(KnownCategories::ARTICLES) &&
          !suggestions.empty()) {
        return &suggestions[0];
      }
      return nullptr;
    }

    for (const ContentSuggestion& suggestion : suggestions) {
      if (suggestion.notification_extra()) {
        return &suggestion;
      }
    }
    return nullptr;
  }

  void AppStatusChanged(base::android::ApplicationState state) {
    if (variations::GetVariationParamByFeatureAsBool(
            kNotificationsFeature, kNotificationsKeepWhenFrontmostParam,
            false)) {
      return;
    }
    if (!ShouldNotifyInState(state)) {
      ContentSuggestionsNotificationHelper::HideAllNotifications(
          CONTENT_SUGGESTIONS_HIDE_FRONTMOST);
    }
  }

  void ImageFetched(const ContentSuggestion::ID& id,
                    const GURL& url,
                    const base::string16& title,
                    const base::string16& text,
                    base::Time timeout_at,
                    const gfx::Image& image) {
    if (!ShouldNotifyInState(app_status_listener_.GetState())) {
      return;  // Became foreground while we were fetching the image; forget it.
    }
    // check if suggestion is still valid.
    DVLOG(1) << "Fetched " << image.Size().width() << "x"
             << image.Size().height() << " image for " << url.spec();
    ConsumeQuota(profile_->GetPrefs());
    int priority = variations::GetVariationParamByFeatureAsInt(
        kNotificationsFeature, kNotificationsPriorityParam,
        kNotificationsDefaultPriority);
    if (ContentSuggestionsNotificationHelper::SendNotification(
            id, url, title, text, CropSquare(image), timeout_at, priority)) {
      RecordContentSuggestionsNotificationImpression(
          id.category().IsKnownCategory(KnownCategories::ARTICLES)
              ? CONTENT_SUGGESTIONS_ARTICLE
              : CONTENT_SUGGESTIONS_NONARTICLE);
    }
  }

  ContentSuggestionsService* const service_;
  Profile* const profile_;
  base::android::ApplicationStatusListener app_status_listener_;

  base::WeakPtrFactory<NotifyingObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotifyingObserver);
};

ContentSuggestionsNotifierService::ContentSuggestionsNotifierService(
    Profile* profile,
    ContentSuggestionsService* suggestions)
    : observer_(base::MakeUnique<NotifyingObserver>(suggestions, profile)) {
  ContentSuggestionsNotificationHelper::FlushCachedMetrics();
  suggestions->AddObserver(observer_.get());
}

ContentSuggestionsNotifierService::~ContentSuggestionsNotifierService() =
    default;

void ContentSuggestionsNotifierService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kContentSuggestionsConsecutiveIgnoredPrefName, 0);
  registry->RegisterIntegerPref(prefs::kContentSuggestionsNotificationsSentDay,
                                0);
  registry->RegisterIntegerPref(
      prefs::kContentSuggestionsNotificationsSentCount, 0);

  // TODO(sfiera): remove after M62; no longer (and never really) used.
  registry->RegisterStringPref(kNotificationIDWithinCategory, std::string());
}
