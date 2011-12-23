// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_SWITCHES_H_
#define ASH_ASH_SWITCHES_H_
#pragma once

#include "ash/ash_export.h"

namespace aura_shell {
namespace switches {

// Please keep alphabetized.
ASH_EXPORT extern const char kAuraNoShadows[];
ASH_EXPORT extern const char kAuraTranslucentFrames[];
ASH_EXPORT extern const char kAuraViewsAppList[];
ASH_EXPORT extern const char kAuraWindowMode[];
ASH_EXPORT extern const char kAuraWindowModeCompact[];
ASH_EXPORT extern const char kAuraWindowModeNormal[];
ASH_EXPORT extern const char kAuraWorkspaceManager[];

// Utilities for testing multi-valued switches.
ASH_EXPORT bool IsAuraWindowModeCompact();

}  // namespace switches
}  // namespace aura_shell

#endif  // ASH_ASH_SWITCHES_H_
