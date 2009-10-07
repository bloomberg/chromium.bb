// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_APP_NAMES_H_
#define CHROME_COMMON_MAC_APP_NAMES_H_

#if !defined(UNIT_TEST)
#error Don't use this file outside of tests
#endif

#if defined(GOOGLE_CHROME_BUILD)
#define MAC_BROWSER_APP_NAME "Google Chrome.app"
#define MAC_FRAMEWORK_NAME "Google Chrome Framework.framework"
#elif defined(CHROMIUM_BUILD)
#define MAC_BROWSER_APP_NAME "Chromium.app"
#define MAC_FRAMEWORK_NAME "Chromium Framework.framework"
#else
#error Not sure what the branding is!
#endif

#endif  // CHROME_COMMON_MAC_APP_NAMES_H_
