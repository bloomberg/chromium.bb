// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONSTANTS_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONSTANTS_H_

#include <stddef.h>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/shadow_value.h"

namespace app_list {

// TODO(crbug.com/854836): Move everything here to app_list_config.h.
ASH_PUBLIC_EXPORT extern const SkColor kContentsBackgroundColor;

ASH_PUBLIC_EXPORT extern const SkColor kLabelBackgroundColor;
ASH_PUBLIC_EXPORT extern const SkColor kBottomSeparatorColor;
ASH_PUBLIC_EXPORT extern const SkColor kDialogSeparatorColor;

ASH_PUBLIC_EXPORT extern const SkColor kHighlightedColor;
ASH_PUBLIC_EXPORT extern const SkColor kGridSelectedColor;
ASH_PUBLIC_EXPORT extern const SkColor kAnswerCardSelectedColor;

ASH_PUBLIC_EXPORT extern const SkColor kPagerHoverColor;
ASH_PUBLIC_EXPORT extern const SkColor kPagerNormalColor;
ASH_PUBLIC_EXPORT extern const SkColor kPagerSelectedColor;

ASH_PUBLIC_EXPORT extern const int kHorizontalPagePreferredHeight;

ASH_PUBLIC_EXPORT extern const SkColor kFolderTitleColor;
ASH_PUBLIC_EXPORT extern const SkColor kFolderTitleHintTextColor;
ASH_PUBLIC_EXPORT extern const SkColor kFolderShadowColor;
ASH_PUBLIC_EXPORT extern const float kFolderBubbleOpacity;

ASH_PUBLIC_EXPORT extern const SkColor kCardBackgroundColor;

ASH_PUBLIC_EXPORT extern const int kPageTransitionDurationInMs;
ASH_PUBLIC_EXPORT extern const int kPageTransitionDurationDampening;
ASH_PUBLIC_EXPORT extern const int kOverscrollPageTransitionDurationMs;
ASH_PUBLIC_EXPORT extern const int kFolderTransitionInDurationMs;
ASH_PUBLIC_EXPORT extern const int kFolderTransitionOutDurationMs;
ASH_PUBLIC_EXPORT extern const int kCustomPageCollapsedHeight;
ASH_PUBLIC_EXPORT extern const gfx::Tween::Type kFolderFadeInTweenType;
ASH_PUBLIC_EXPORT extern const gfx::Tween::Type kFolderFadeOutTweenType;

ASH_PUBLIC_EXPORT extern const int kNumStartPageTiles;
ASH_PUBLIC_EXPORT extern const size_t kMaxSearchResults;

ASH_PUBLIC_EXPORT extern const int kReorderDroppingCircleRadius;

ASH_PUBLIC_EXPORT extern const int kAppsGridPadding;
ASH_PUBLIC_EXPORT extern const int kAppsGridLeftRightPadding;
ASH_PUBLIC_EXPORT extern const int kBottomSeparatorLeftRightPadding;
ASH_PUBLIC_EXPORT extern const int kBottomSeparatorBottomPadding;
ASH_PUBLIC_EXPORT extern const int kSearchBoxPeekingBottomPadding;
ASH_PUBLIC_EXPORT extern const int kSearchBoxBottomPadding;

ASH_PUBLIC_EXPORT extern const size_t kMaxFolderPages;
ASH_PUBLIC_EXPORT extern const size_t kMaxFolderItemsPerPage;
ASH_PUBLIC_EXPORT extern const size_t kMaxFolderNameChars;

ASH_PUBLIC_EXPORT extern const ui::ResourceBundle::FontStyle kItemTextFontStyle;

ASH_PUBLIC_EXPORT extern const float kAllAppsOpacityStartPx;
ASH_PUBLIC_EXPORT extern const float kAllAppsOpacityEndPx;

ASH_PUBLIC_EXPORT extern const int kSearchResultTitleTextSizeDelta;
ASH_PUBLIC_EXPORT extern const ui::ResourceBundle::FontStyle
    kSearchResultTitleFontStyle;

// The different ways that the app list can transition from PEEKING to
// FULLSCREEN_ALL_APPS. These values are written to logs.  New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum AppListPeekingToFullscreenSource {
  kSwipe = 0,
  kExpandArrow = 1,
  kMousepadScroll = 2,
  kMousewheelScroll = 3,
  kMaxPeekingToFullscreen = 4,
};

// The different ways the app list can be shown. These values are written to
// logs.  New enum values can be added, but existing enums must never be
// renumbered or deleted and reused.
enum AppListShowSource {
  kSearchKey = 0,
  kShelfButton = 1,
  kSwipeFromShelf = 2,
  kTabletMode = 3,
  kMaxAppListToggleMethod = 4,
};

// The two versions of folders. These values are written to logs.  New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum AppListFolderOpened {
  kOldFolders = 0,
  kFullscreenAppListFolders = 1,
  kMaxFolderOpened = 2,
};

// The valid AppListState transitions. These values are written to logs.  New
// enum values can be added, but existing enums must never be renumbered or
// deleted and reused. If adding a state transition, add it to the switch
// statement in AppListView::GetAppListStateTransitionSource.
enum AppListStateTransitionSource {
  kFullscreenAllAppsToClosed = 0,
  kFullscreenAllAppsToFullscreenSearch = 1,
  kFullscreenAllAppsToPeeking = 2,
  kFullscreenSearchToClosed = 3,
  kFullscreenSearchToFullscreenAllApps = 4,
  kHalfToClosed = 5,
  KHalfToFullscreenSearch = 6,
  kHalfToPeeking = 7,
  kPeekingToClosed = 8,
  kPeekingToFullscreenAllApps = 9,
  kPeekingToHalf = 10,
  kMaxAppListStateTransition = 11,
};

// The different ways to change pages in the app list's app grid. These values
// are written to logs.  New enum values can be added, but existing enums must
// never be renumbered or deleted and reused.
enum AppListPageSwitcherSource {
  kTouchPageIndicator = 0,
  kClickPageIndicator = 1,
  kSwipeAppGrid = 2,
  kFlingAppGrid = 3,
  kMouseWheelScroll = 4,
  kMousePadScroll = 5,
  kDragAppToBorder = 6,
  kMaxAppListPageSwitcherSource = 7,
};

// The different ways to move an app in app list's apps grid. These values are
// written to logs. New enum values can be added, but existing enums must never
// be renumbered or deleted and reused.
enum AppListAppMovingType {
  kMoveIntoFolder = 0,
  kMoveOutOfFolder = 1,
  kMoveIntoAnotherFolder = 2,
  kReorderInFolder = 3,
  kReorderInTopLevel = 4,
  kMaxAppListAppMovingType = 5,
};

ASH_PUBLIC_EXPORT extern const char kAppListAppLaunched[];
ASH_PUBLIC_EXPORT extern const char kAppListAppLaunchedFullscreen[];
ASH_PUBLIC_EXPORT extern const char kAppListAppMovingType[];
ASH_PUBLIC_EXPORT extern const char kAppListCreationTimeHistogram[];
ASH_PUBLIC_EXPORT extern const char kAppListStateTransitionSourceHistogram[];
ASH_PUBLIC_EXPORT extern const char kAppListPageSwitcherSourceHistogram[];
ASH_PUBLIC_EXPORT extern const char kAppListFolderOpenedHistogram[];
ASH_PUBLIC_EXPORT extern const char kAppListPeekingToFullscreenHistogram[];
ASH_PUBLIC_EXPORT extern const char kAppListToggleMethodHistogram[];
ASH_PUBLIC_EXPORT extern const char kPageOpenedHistogram[];
ASH_PUBLIC_EXPORT extern const char kNumberOfAppsInFoldersHistogram[];
ASH_PUBLIC_EXPORT extern const char kNumberOfFoldersHistogram[];
ASH_PUBLIC_EXPORT extern const char kNumberOfPagesHistogram[];
ASH_PUBLIC_EXPORT extern const char kNumberOfPagesNotFullHistogram[];

ASH_PUBLIC_EXPORT extern const char kSearchResultOpenDisplayTypeHistogram[];
ASH_PUBLIC_EXPORT extern const char kSearchQueryLength[];
ASH_PUBLIC_EXPORT extern const char kSearchResultDistanceFromOrigin[];

ASH_PUBLIC_EXPORT extern const int kSearchTileHeight;

// Returns the shadow values for a view at |z_height|.
ASH_PUBLIC_EXPORT gfx::ShadowValue GetShadowForZHeight(int z_height);

}  // namespace app_list

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONSTANTS_H_
