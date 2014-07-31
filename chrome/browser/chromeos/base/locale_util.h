// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for locale change.

#ifndef CHROME_BROWSER_CHROMEOS_BASE_LOCALE_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_BASE_LOCALE_UTIL_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {

template <typename T>
class Callback;

}  // namespace base

namespace chromeos {
namespace locale_util {

// This callback is called on UI thread, when ReloadLocaleResources() is
// completed on BlockingPool.
// Arguments:
//   locale - (copy of) locale argument to SwitchLanguage(). Expected locale.
//   loaded_locale - actual locale name loaded.
//   success - if locale load succeeded.
// (const std::string* locale, const std::string* loaded_locale, bool success)
typedef base::Callback<void(const std::string&, const std::string&, bool)>
    SwitchLanguageCallback;

// This function updates input methods only if requested. In general, you want
// |enable_locale_keyboard_layouts = true|.
// Note: in case of |enable_locale_keyboard_layouts = false|, the input method
// currently in use may not be supported by the new locale. Using the new locale
// with an unsupported input method may lead to undefined behavior. Use
// |enable_locale_keyboard_layouts = false| with caution.
// Note 2: |login_layouts_only = true| enables only login-capable layouts.
void SwitchLanguage(const std::string& locale,
                    const bool enable_locale_keyboard_layouts,
                    const bool login_layouts_only,
                    scoped_ptr<SwitchLanguageCallback> callback);

}  // namespace locale_util
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BASE_LOCALE_UTIL_H_
