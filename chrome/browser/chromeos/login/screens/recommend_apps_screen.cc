// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/recommend_apps_screen.h"

namespace chromeos {

RecommendAppsScreen::RecommendAppsScreen(
    RecommendAppsScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(OobeScreen::SCREEN_RECOMMEND_APPS),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);

  view_->Bind(this);
  view_->AddObserver(this);
}

RecommendAppsScreen::~RecommendAppsScreen() {
  if (view_) {
    view_->Bind(nullptr);
    view_->RemoveObserver(this);
  }
}

void RecommendAppsScreen::Show() {
  view_->Show();

  recommend_apps_fetcher_ = std::make_unique<RecommendAppsFetcher>(view_);
}

void RecommendAppsScreen::Hide() {
  view_->Hide();
}

void RecommendAppsScreen::OnSkip() {
  exit_callback_.Run(Result::SKIPPED);
}

void RecommendAppsScreen::OnRetry() {
  recommend_apps_fetcher_->Retry();
}

void RecommendAppsScreen::OnInstall() {
  exit_callback_.Run(Result::SELECTED);
}

void RecommendAppsScreen::OnViewDestroyed(RecommendAppsScreenView* view) {
  DCHECK_EQ(view, view_);
  view_->RemoveObserver(this);
  view_ = nullptr;
}

}  // namespace chromeos
