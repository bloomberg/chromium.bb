// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_CAPTION_UTIL_H_
#define CHROME_BROWSER_ACCESSIBILITY_CAPTION_UTIL_H_

#include "ui/native_theme/caption_style.h"

class PrefService;

namespace captions {

base::Optional<ui::CaptionStyle> GetCaptionStyleFromUserSettings(
    PrefService* prefs,
    bool record_metrics);

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_CAPTION_UTIL_H_
