// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_constants.h"

#include "build/build_config.h"
#include "ui/gfx/color_palette.h"

namespace app_list {

// The UMA histogram that logs usage of suggested and regular apps.
const char kAppListAppLaunched[] = "Apps.AppListAppLaunched";

// The UMA histogram that logs usage of suggested and regular apps while the
// fullscreen launcher is enabled.
const char kAppListAppLaunchedFullscreen[] =
    "Apps.AppListAppLaunchedFullscreen";

// The UMA histogram that logs different ways to move an app in app list's apps
// grid.
const char kAppListAppMovingType[] = "Apps.AppListAppMovingType";

// The UMA histogram that logs the creation time of the AppListView.
const char kAppListCreationTimeHistogram[] = "Apps.AppListCreationTime";

// The UMA histogram that logs usage of state transitions in the new
// app list UI.
const char kAppListStateTransitionSourceHistogram[] =
    "Apps.AppListStateTransitionSource";

// The UMA histogram that logs the source of vertical page switcher usage in the
// app list.
const char kAppListPageSwitcherSourceHistogram[] =
    "Apps.AppListPageSwitcherSource";

// The UMA histogram that logs usage of the original and redesigned folders.
const char kAppListFolderOpenedHistogram[] = "Apps.AppListFolderOpened";

// The UMA histogram that logs how the app list transitions from peeking to
// fullscreen.
const char kAppListPeekingToFullscreenHistogram[] =
    "Apps.AppListPeekingToFullscreenSource";

// The UMA histogram that logs how the app list is shown.
const char kAppListToggleMethodHistogram[] = "Apps.AppListShowSource";

// The UMA histogram that logs which page gets opened by the user.
const char kPageOpenedHistogram[] = "Apps.AppListPageOpened";

// The UMA histogram that logs how many apps users have in folders.
const char kNumberOfAppsInFoldersHistogram[] =
    "Apps.AppsInFolders.FullscreenAppListEnabled";

// The UMA histogram that logs how many folders users have.
const char kNumberOfFoldersHistogram[] = "Apps.NumberOfFolders";

// The UMA histogram that logs how many pages users have in top level apps grid.
const char kNumberOfPagesHistogram[] = "Apps.NumberOfPages";

// The UMA histogram that logs how many pages with empty slots users have in top
// level apps grid.
const char kNumberOfPagesNotFullHistogram[] = "Apps.NumberOfPagesNotFull";

// The UMA histogram that logs the type of search result opened.
const char kSearchResultOpenDisplayTypeHistogram[] =
    "Apps.AppListSearchResultOpenDisplayType";

// The UMA histogram that logs how long the search query was when a result was
// opened.
const char kSearchQueryLength[] = "Apps.AppListSearchQueryLength";

// The UMA histogram that logs the Manhattan distance from the origin of the
// search results to the selected result.
const char kSearchResultDistanceFromOrigin[] =
    "Apps.AppListSearchResultDistanceFromOrigin";

// The height of tiles in search result.
const int kSearchTileHeight = 90;

gfx::ShadowValue GetShadowForZHeight(int z_height) {
  if (z_height <= 0)
    return gfx::ShadowValue();

  switch (z_height) {
    case 1:
      return gfx::ShadowValue(gfx::Vector2d(0, 1), 4,
                              SkColorSetARGB(0x4C, 0, 0, 0));
    case 2:
      return gfx::ShadowValue(gfx::Vector2d(0, 2), 8,
                              SkColorSetARGB(0x33, 0, 0, 0));
    default:
      return gfx::ShadowValue(gfx::Vector2d(0, 8), 24,
                              SkColorSetARGB(0x3F, 0, 0, 0));
  }
}

}  // namespace app_list
