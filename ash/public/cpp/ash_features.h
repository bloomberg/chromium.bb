// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_FEATURES_H_
#define ASH_PUBLIC_CPP_ASH_FEATURES_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/feature_list.h"

namespace ash {
namespace features {

// Enables new system menu.
ASH_PUBLIC_EXPORT extern const base::Feature kAshNewSystemMenu;

ASH_PUBLIC_EXPORT bool IsNewSystemMenuEnabled();

}  // namespace features
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_FEATURES_H_
