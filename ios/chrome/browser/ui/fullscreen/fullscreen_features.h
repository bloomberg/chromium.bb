// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_FEATURES_H_

#include "base/feature_list.h"

namespace fullscreen {
namespace features {

// Used to control whether fullscreen should be implemented by adjusting the
// WKWebView's contentInset rather than its frame.
extern const base::Feature kFullscreenContentInset;

}  // namespace features
}  // namespace fullscreen

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_FEATURES_H_
