// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PROPERTIES_H_
#define ASH_WM_WINDOW_PROPERTIES_H_
#pragma once

namespace aura_shell {
namespace internal {

// Shell-specific window property keys.

// Alphabetical sort.

// A property key for a value from ShadowType describing the drop shadow that
// should be displayed under the window.  If unset, no shadow is displayed.
extern const char kShadowTypeKey[];

// Alphabetical sort.

}  // namespace internal
}  // namespace aura_shell

#endif  // ASH_WM_WINDOW_PROPERTIES_H_
