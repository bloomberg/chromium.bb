// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RENDERER_PREFERENCES_UTIL_H_
#define CONTENT_PUBLIC_COMMON_RENDERER_PREFERENCES_UTIL_H_

#include "content/common/content_export.h"

namespace content {

struct RendererPreferences;

// Updates |prefs| from system settings.
CONTENT_EXPORT void UpdateFontRendererPreferencesFromSystemSettings(
    RendererPreferences* prefs);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RENDERER_PREFERENCES_UTIL_H_
