// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/crostini_app_result.h"

#include "ash/public/cpp/app_list/app_list_constants.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_context_menu.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_icon_loader.h"

namespace app_list {

CrostiniAppResult::CrostiniAppResult(Profile* profile,
                                     const std::string& app_id,
                                     AppListControllerDelegate* controller,
                                     bool is_recommendation)
    : AppResult(profile, app_id, controller, is_recommendation) {
  set_id(app_id);

  icon_loader_.reset(new CrostiniAppIconLoader(
      profile, GetPreferredIconDimension(display_type()), this));
  icon_loader_->FetchImage(app_id);
}

CrostiniAppResult::~CrostiniAppResult() = default;

void CrostiniAppResult::Open(int event_flags) {
  LaunchCrostiniApp(profile(), app_id());
}

std::unique_ptr<ChromeSearchResult> CrostiniAppResult::Duplicate() const {
  auto copy = std::make_unique<CrostiniAppResult>(
      profile(), app_id(), controller(),
      display_type() == ash::SearchResultDisplayType::kRecommendation);
  copy->set_title(title());
  copy->set_title_tags(title_tags());
  copy->set_relevance(relevance());
  return copy;
}

void CrostiniAppResult::GetContextMenuModel(GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<CrostiniAppContextMenu>(profile(), app_id(),
                                                           controller());
  context_menu_->GetMenuModel(std::move(callback));
}

void CrostiniAppResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

void CrostiniAppResult::OnAppImageUpdated(const std::string& app_id,
                                          const gfx::ImageSkia& image) {
  SetIcon(image);
}

AppContextMenu* CrostiniAppResult::GetAppContextMenu() {
  return context_menu_.get();
}

}  // namespace app_list
