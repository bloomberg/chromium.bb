// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_FEATURES_H_
#define ASH_PUBLIC_CPP_ASH_FEATURES_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/feature_list.h"

namespace ash {
namespace features {

// Enables the docked (a.k.a. picture-in-picture) magnifier.
// TODO(afakhry): Remove this after the feature is fully launched.
// https://crbug.com/709824.
ASH_PUBLIC_EXPORT extern const base::Feature kDockedMagnifier;

// Enables the keyboard shortcut viewer.
// TODO(wutao): Remove this after the feature is fully launched.
// https://crbug.com/755448.
ASH_PUBLIC_EXPORT extern const base::Feature kKeyboardShortcutViewer;

// Enables new system menu.
ASH_PUBLIC_EXPORT extern const base::Feature kSystemTrayUnified;

ASH_PUBLIC_EXPORT bool IsDockedMagnifierEnabled();

ASH_PUBLIC_EXPORT bool IsKeyboardShortcutViewerEnabled();

ASH_PUBLIC_EXPORT bool IsSystemTrayUnifiedEnabled();

}  // namespace features
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_FEATURES_H_
