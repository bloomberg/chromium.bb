// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_UTIL_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"

namespace chromeos {
namespace input_method {

// The list of language that do not have associated input methods in IBus.
// For these languages, we associate input methods here.
const struct ExtraLanguage {
  const char* language_code;
  const char* input_method_id;
} kExtraLanguages[] = {
  { "id", "xkb:us::eng" }, // For Indonesian, use US keyboard layout.
  // The code "fil" comes from app/l10_util.cc.
  { "fil", "xkb:us::eng" },  // For Filipino, use US keyboard layout.
  // The code "es-419" comes from app/l10_util.cc.
  // For Spanish in Latin America, use Latin American keyboard layout.
  { "es-419", "xkb:latam::spa" },
};
// TODO(yusukes): Move |kExtraLanguages| to input_method_util.cc.

// Converts a string sent from IBus IME engines, which is written in English,
// into Chrome's string ID, then pulls internationalized resource string from
// the resource bundle and returns it. These functions are not thread-safe.
// Non-UI threads are not allowed to call them.
std::wstring GetString(const std::string& english_string);
std::string GetStringUTF8(const std::string& english_string);
string16 GetStringUTF16(const std::string& english_string);

// This method is ONLY for unit testing. Returns true if the given string is
// supported (i.e. the string is associated with a resource ID).
bool StringIsSupported(const std::string& english_string);

// Normalizes the language code and returns the normalized version.  The
// function normalizes the given language code to be compatible with the
// one used in Chrome's application locales. Otherwise, returns the
// given language code as-is.
//
// Examples:
//
// - "zh_CN" => "zh-CN" (Use - instead of _)
// - "jpn"   => "ja"    (Use two-letter code)
// - "t"     => "t"     (Return as-is if unknown)
std::string NormalizeLanguageCode(const std::string& language_code);

// Returns true if the given input method id is for a keyboard layout.
bool IsKeyboardLayout(const std::string& input_method_id);

// Gets the language code from the given input method descriptor.  This
// encapsulates differences between the language codes used in
// InputMethodDescriptor and Chrome's application locale codes.
std::string GetLanguageCodeFromDescriptor(
    const InputMethodDescriptor& descriptor);

// Gets the keyboard layout name from the given input method ID.
// If the ID is invalid, an empty string will be returned.
// This function only supports xkb layouts.
//
// Examples:
//
// "xkb:us::eng"       => "us"
// "xkb:us:dvorak:eng" => "us(dvorak)"
// "xkb:gb::eng"       => "gb"
// "pinyin"            => "us" (because Pinyin uses US keyboard layout)
std::string GetKeyboardLayoutName(const std::string& input_method_id);

// Gets the ID for the keyboard overlay from the given input method ID.
// If the ID is invalid, an empty string will be returned.
//
// Examples:
//
// "xkb:us::eng"       => "en_US"
// "xkb:us:dvorak:eng" => "en_US_dvorak"
// "xkb:gb::eng"       => "en_GB"
// "pinyin"            => "zh_CN"
// "mozc"              => "ja"
std::string GetKeyboardOverlayId(const std::string& input_method_id);

// Converts an input method ID to a language code of the IME. Returns "Eng"
// when |input_method_id| is unknown.
// Example: "hangul" => "ko"
std::string GetLanguageCodeFromInputMethodId(
    const std::string& input_method_id);

// Converts an input method ID to a display name of the IME. Returns
// an empty strng when |input_method_id| is unknown.
// Examples: "pinyin" => "Pinyin"
//           "m17n:ar:kbd" => "kbd (m17n)"
std::string GetInputMethodDisplayNameFromId(const std::string& input_method_id);

// Converts an input method ID to an input method descriptor. Returns NULL
// when |input_method_id| is unknown.
// Example: "pinyin" => { id: "pinyin", display_name: "Pinyin",
//                        keyboard_layout: "us", language_code: "zh" }
const chromeos::InputMethodDescriptor* GetInputMethodDescriptorFromId(
    const std::string& input_method_id);

// Converts a language code to a language display name, using the
// current application locale. MaybeRewriteLanguageName() is called
// internally.
// Examples: "fi"    => "Finnish"
//           "en-US" => "English (United States)"
string16 GetLanguageDisplayNameFromCode(const std::string& language_code);

// Converts a language code to a language native display name.
// MaybeRewriteLanguageName() is called internally.
// Examples: "fi"    => "suomi" (rather than Finnish)
//           "en-US" => "English (United States)"
string16 GetLanguageNativeDisplayNameFromCode(const std::string& language_code);

// Sorts the given language codes by their corresponding language names,
// using the unicode string comparator. Uses unstable sorting.
void SortLanguageCodesByNames(std::vector<std::string>* language_codes);

// Used for EnableInputMethods() etc.
enum InputMethodType {
  kKeyboardLayoutsOnly,
  kAllInputMethods,
};

// Gets input method IDs that belong to |language_code|.
// If |type| is |kKeyboardLayoutsOnly|, the function does not return input
// methods that are not for keybord layout switching. Returns true on success.
// Note that the function might return false or |language_code| is unknown.
//
// The retured input method IDs are sorted per
// chromeos/platform/assets/input_methods/whitelist.txt.
bool GetInputMethodIdsFromLanguageCode(
    const std::string& language_code,
    InputMethodType type,
    std::vector<std::string>* out_input_method_ids);

// Enables input methods (e.g. Chinese, Japanese) and keyboard layouts (e.g.
// US qwerty, US dvorak, French azerty) that are necessary for the language code
// and then switches to |initial_input_method_id| if the string is not empty.
// For example, if |language_code| is "en-US", US qwerty and US dvorak layouts
// would be enabled. Likewise, for Germany locale, US qwerty layout and several
// keyboard layouts for Germany would be enabled.
// If |type| is kAllInputMethods, all keyboard layouts and all input methods
// are enabled. If it's kKeyboardLayoutsOnly, only keyboard layouts are enabled.
// For example, for Japanese, xkb:jp::jpn is enabled when kKeyboardLayoutsOnly,
// and xkb:jp::jpn, mozc, mozc-jp, mozc-dv are enabled when kAllInputMethods.
//
// Note that this function does not save the input methods in the user's
// preferences, as this function is designed for the login screen and the
// screen locker, where we shouldn't change the user's preferences.
void EnableInputMethods(const std::string& language_code, InputMethodType type,
                        const std::string& initial_input_method_id);

// Returns the input method ID of the hardware keyboard.
std::string GetHardwareInputMethodId();

// Returns the fallback input method descriptor (the very basic US
// keyboard). This function is mostly used for testing, but may be used
// as the fallback, when there is no other choice.
InputMethodDescriptor GetFallbackInputMethodDescriptor();

// This function should be called when Chrome's application locale is
// changed, so that the internal maps of this library is reloaded.
void OnLocaleChanged();

bool GetInputMethodIdsFromLanguageCodeInternal(
    const std::multimap<std::string, std::string>& language_code_to_ids,
    const std::string& normalized_language_code,
    InputMethodType type,
    std::vector<std::string>* out_input_method_ids);

void ReloadInternalMaps();

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_UTIL_H_
