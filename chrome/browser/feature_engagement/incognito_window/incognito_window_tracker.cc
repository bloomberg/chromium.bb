// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/incognito_window/incognito_window_tracker.h"

#include "base/time/time.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/views/feature_promos/incognito_window_promo_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"

namespace {

constexpr int kDefaultPromoShowTimeInHours = 2;

AppMenuButton* GetAppMenuButton() {
  auto* browser = BrowserView::GetBrowserViewForBrowser(
      BrowserList::GetInstance()->GetLastActive());
  DCHECK(browser);
  DCHECK(browser->IsActive());
  DCHECK(browser->toolbar());
  DCHECK(browser->toolbar()->app_menu_button());
  return browser->toolbar()->app_menu_button();
}

}  // namespace

namespace feature_engagement {

IncognitoWindowTracker::IncognitoWindowTracker(
    Profile* profile,
    SessionDurationUpdater* session_duration_updater)
    : FeatureTracker(profile,
                     session_duration_updater,
                     &kIPHIncognitoWindowFeature,
                     base::TimeDelta::FromHours(kDefaultPromoShowTimeInHours)),
      incognito_promo_observer_(this) {}

IncognitoWindowTracker::IncognitoWindowTracker(
    SessionDurationUpdater* session_duration_updater)
    : IncognitoWindowTracker(nullptr, session_duration_updater) {}

IncognitoWindowTracker::~IncognitoWindowTracker() = default;

void IncognitoWindowTracker::OnIncognitoWindowOpened() {
  GetTracker()->NotifyEvent(events::kIncognitoWindowOpened);
}

void IncognitoWindowTracker::OnBrowsingDataCleared() {
  const auto severity = GetAppMenuButton()->severity();
  if (severity == AppMenuIconController::Severity::NONE && ShouldShowPromo())
    ShowPromo();
}

void IncognitoWindowTracker::OnPromoClosed() {
  GetTracker()->Dismissed(kIPHIncognitoWindowFeature);
}

void IncognitoWindowTracker::OnSessionTimeMet() {
  GetTracker()->NotifyEvent(events::kIncognitoWindowSessionTimeMet);
}

void IncognitoWindowTracker::ShowPromo() {
  DCHECK(!incognito_promo_);
  auto* app_menu_button = GetAppMenuButton();

  // Owned by its native widget. Will be destroyed when its widget is destroyed.
  incognito_promo_ =
      IncognitoWindowPromoBubbleView::CreateOwned(app_menu_button);
  views::Widget* widget = incognito_promo_->GetWidget();
  incognito_promo_observer_.Add(widget);
  app_menu_button->SetIsProminent(true);
}

void IncognitoWindowTracker::OnWidgetDestroying(views::Widget* widget) {
  OnPromoClosed();

  if (incognito_promo_observer_.IsObserving(widget)) {
    incognito_promo_observer_.Remove(widget);
    GetAppMenuButton()->SetIsProminent(false);
  }
}

}  // namespace feature_engagement
