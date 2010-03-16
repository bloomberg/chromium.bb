// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_IDLE_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_IDLE_API_CONSTANTS_H_

namespace extension_idle_api_constants {

// Keys.
extern const wchar_t kSecondsKey[];
extern const wchar_t kStateKey[];

// Events.
extern const char kOnStateChanged[];

// States
extern const wchar_t kStateActive[];
extern const wchar_t kStateIdle[];
extern const wchar_t kStateLocked[];

};  // namespace extension_idle_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_IDLE_API_CONSTANTS_H_
