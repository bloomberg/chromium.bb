// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/internal_app_result.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/internal_app_metadata.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "chrome/browser/ui/ash/ksv/keyboard_shortcut_viewer_util.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace app_list {

namespace {

gfx::ImageSkia GetIconForInternalAppId(const std::string& app_id) {
  const int resource_id = GetIconResourceIdByAppId(app_id);
  if (resource_id == 0)
    return gfx::ImageSkia();

  gfx::ImageSkia* source =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  return gfx::ImageSkiaOperations::CreateResizedImage(
      *source, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kTileIconSize, kTileIconSize));
}

}  // namespace

InternalAppResult::InternalAppResult(Profile* profile,
                                     const std::string& app_id,
                                     AppListControllerDelegate* controller,
                                     bool is_recommendation)
    : AppResult(profile, app_id, controller, is_recommendation) {
  set_id(app_id);
  set_result_type(ResultType::kInternalApp);

  gfx::ImageSkia icon = GetIconForInternalAppId(app_id);
  if (!icon.isNull())
    SetIcon(icon);
}

InternalAppResult::~InternalAppResult() {}

void InternalAppResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

void InternalAppResult::Open(int event_flags) {
  // Record the search metric if the result is not a suggested app.
  if (display_type() != DisplayType::kRecommendation)
    RecordHistogram(APP_SEARCH_RESULT);

  if (id() == kInternalAppIdKeyboardShortcutViewer)
    keyboard_shortcut_viewer_util::ShowKeyboardShortcutViewer();
}

std::unique_ptr<SearchResult> InternalAppResult::Duplicate() const {
  auto copy = std::make_unique<InternalAppResult>(
      profile(), app_id(), controller(),
      display_type() == DisplayType::kRecommendation);
  copy->set_title(title());
  copy->set_title_tags(title_tags());
  copy->set_relevance(relevance());
  return copy;
}

ui::MenuModel* InternalAppResult::GetContextMenuModel() {
  return nullptr;
}

}  // namespace app_list
