// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_
#define CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_

namespace chromeos {

// For Korean IME (ibus-hangul)
const struct HangulKeyboardNameIDPair {
  const wchar_t* keyboard_name;
  const wchar_t* keyboard_id;
} kHangulKeyboardNameIDPairs[] = {
  // We have to sync the IDs with those in ibus-hangul/files/setup/main.py.
  { L"Dubeolsik", L"2" },
  { L"Sebeolsik Final", L"3f" },
  { L"Sebeolsik 390", L"39" },
  { L"Sebeolsik No-shift", L"3s" },
  { L"Sebeolsik 2 set", L"32" },
  // TODO(yusukes): Use generated_resources.grd IDs for |keyboard_name|. Ask
  // jshin first.
};

// For ibus-daemon
// TODO(yusukes): We'll add more "next engine" hot-keys like "Zenkaku_Hankaku"
// (Japanese keyboard specific) and "Right-Alt+Right-Shift".
const wchar_t kHotkeyNextEngine[] = L"Shift+space,Alt+grave";
const wchar_t kHotkeyTrigger[] = L"";  // We don't allow users to disable IBus.

// For Simplified Chinese IME (ibus-pinyin)
// For Traditional Chinese IME (ibus-chewing)
// For Japanese IME (ibus-google-japanese-input)

// TODO(yusukes): Add constants for these components.
}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_

