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

}  // namespace

class ContentSuggestionsNotifierService::NotifyingObserver
    : public ContentSuggestionsService::Observer {
 public:
  NotifyingObserver(ContentSuggestionsService* service,
                    Profile* profile,
                    PrefService* prefs)
      : service_(service),
        profile_(profile),
        prefs_(prefs),
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
    }
    const ContentSuggestion* suggestion = GetSuggestionToNotifyAbout(category);
    if (!suggestion) {
      return;
    }
    base::Time timeout_at = suggestion->notification_extra()
                                ? suggestion->notification_extra()->deadline
                                : base::Time::Max();
    service_->FetchSuggestionImage(
        suggestion->id(),
        base::Bind(&NotifyingObserver::ImageFetched,
                   weak_ptr_factory_.GetWeakPtr(), suggestion->id(),
                   suggestion->url(), suggestion->title(),
                   suggestion->publisher_name(), timeout_at));
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
    // TODO(sfiera): handle concurrent notifications and non-articles properly.
    if (suggestion_id.category().IsKnownCategory(KnownCategories::ARTICLES) &&
        (suggestion_id.id_within_category() ==
         prefs_->GetString(kNotificationIDWithinCategory))) {
      ContentSuggestionsNotificationHelper::HideNotification(
          suggestion_id, CONTENT_SUGGESTIONS_HIDE_EXPIRY);
    }
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
             kContentSuggestionsNotificationsFeature,
             kContentSuggestionsNotificationsAlwaysNotifyParam, false)) {
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
    if (!ShouldNotifyInState(state)) {
      ContentSuggestionsNotificationHelper::HideAllNotifications(
          CONTENT_SUGGESTIONS_HIDE_FRONTMOST);
    }
  }

  void ImageFetched(const ContentSuggestion::ID& id,
                    const GURL& url,
                    const base::string16& title,
                    const base::string16& publisher,
                    base::Time timeout_at,
                    const gfx::Image& image) {
    if (!ShouldNotifyInState(app_status_listener_.GetState())) {
      return;  // Became foreground while we were fetching the image; forget it.
    }
    // check if suggestion is still valid.
    DVLOG(1) << "Fetched " << image.Size().width() << "x"
             << image.Size().height() << " image for " << url.spec();
    prefs_->ClearPref(kNotificationIDWithinCategory);
    if (ContentSuggestionsNotificationHelper::SendNotification(
            id, url, title, publisher, CropSquare(image), timeout_at)) {
      RecordContentSuggestionsNotificationImpression(
          id.category().IsKnownCategory(KnownCategories::ARTICLES)
              ? CONTENT_SUGGESTIONS_ARTICLE
              : CONTENT_SUGGESTIONS_NONARTICLE);
    }
  }

  ContentSuggestionsService* const service_;
  Profile* const profile_;
  PrefService* const prefs_;
  base::android::ApplicationStatusListener app_status_listener_;

  base::WeakPtrFactory<NotifyingObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotifyingObserver);
};

ContentSuggestionsNotifierService::ContentSuggestionsNotifierService(
    Profile* profile,
    ContentSuggestionsService* suggestions,
    PrefService* prefs)
    : observer_(base::MakeUnique<NotifyingObserver>(suggestions,
                                                    profile,
                                                    profile->GetPrefs())) {
  ContentSuggestionsNotificationHelper::FlushCachedMetrics();
  suggestions->AddObserver(observer_.get());
}

ContentSuggestionsNotifierService::~ContentSuggestionsNotifierService() =
    default;

void ContentSuggestionsNotifierService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kNotificationIDWithinCategory, std::string());
  registry->RegisterIntegerPref(
      prefs::kContentSuggestionsConsecutiveIgnoredPrefName, 0);
}
