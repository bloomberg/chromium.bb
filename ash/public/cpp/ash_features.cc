// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"

namespace ash {
namespace features {

const base::Feature kDisplayMoveWindowAccels{"DisplayMoveWindowAccels",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDockedMagnifier{"DockedMagnifier",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kKeyboardShortcutViewer{"KeyboardShortcutViewer",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNewOverviewAnimations{"NewOverviewAnimations",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNewOverviewUi{"NewOverviewUi",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPersistentWindowBounds{"PersistentWindowBounds",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSystemTrayUnified{"SystemTrayUnified",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTrilinearFiltering{"TrilinearFiltering",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kLockScreenNotifications{"LockScreenNotifications",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

bool IsDisplayMoveWindowAccelsEnabled() {
  return base::FeatureList::IsEnabled(kDisplayMoveWindowAccels);
}

bool IsDockedMagnifierEnabled() {
  return base::FeatureList::IsEnabled(kDockedMagnifier);
}

bool IsKeyboardShortcutViewerEnabled() {
  return base::FeatureList::IsEnabled(kKeyboardShortcutViewer);
}

bool IsPersistentWindowBoundsEnabled() {
  return base::FeatureList::IsEnabled(kPersistentWindowBounds);
}

bool IsSystemTrayUnifiedEnabled() {
  return base::FeatureList::IsEnabled(kSystemTrayUnified);
}

bool IsTrilinearFilteringEnabled() {
  static bool use_trilinear_filtering =
      base::FeatureList::IsEnabled(kTrilinearFiltering);
  return use_trilinear_filtering;
}

bool IsLockScreenNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kLockScreenNotifications);
}

}  // namespace features
}  // namespace ash
