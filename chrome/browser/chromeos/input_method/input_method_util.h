// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_UTIL_H_

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chromeos/ime/input_method_descriptor.h"

namespace chromeos {
namespace input_method {

class InputMethodDelegate;

enum InputMethodType {
  kKeyboardLayoutsOnly,
  kAllInputMethods,
};

// A class which provides miscellaneous input method utility functions.
class InputMethodUtil {
 public:
  // |supported_input_methods| is a list of all input methods supported,
  // including ones not active. The list is used to initialize member variables
  // in this class.
  InputMethodUtil(InputMethodDelegate* delegate,
                  scoped_ptr<InputMethodDescriptors> supported_input_methods);
  ~InputMethodUtil();

  // Converts a string sent from IBus IME engines, which is written in English,
  // into Chrome's string ID, then pulls internationalized resource string from
  // the resource bundle and returns it. These functions are not thread-safe.
  // Non-UI threads are not allowed to call them.
  string16 TranslateString(const std::string& english_string) const;

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
  std::string GetKeyboardLayoutName(const std::string& input_method_id) const;

  // Converts an input method ID to a language code of the IME. Returns "Eng"
  // when |input_method_id| is unknown.
  // Example: "hangul" => "ko"
  std::string GetLanguageCodeFromInputMethodId(
      const std::string& input_method_id) const;

  // Converts an input method ID to a display name of the IME. Returns
  // an empty strng when |input_method_id| is unknown.
  // Examples: "pinyin" => "Pinyin"
  std::string GetInputMethodDisplayNameFromId(
      const std::string& input_method_id) const;

  string16 GetInputMethodShortName(
      const InputMethodDescriptor& input_method) const;
  string16 GetInputMethodMediumName(
      const InputMethodDescriptor& input_method) const;
  string16 GetInputMethodLongName(
      const InputMethodDescriptor& input_method) const;

  // Converts an input method ID to an input method descriptor. Returns NULL
  // when |input_method_id| is unknown.
  // Example: "pinyin" => { id: "pinyin", display_name: "Pinyin",
  //                        keyboard_layout: "us", language_code: "zh" }
  const InputMethodDescriptor* GetInputMethodDescriptorFromId(
      const std::string& input_method_id) const;

  // Converts an XKB layout ID to an input method descriptor. Returns NULL when
  // |xkb_id| is unknown.
  // Example: "us(dvorak)" => {
  //              id: "xkb:us:dvorak:eng", display_name: "US Dvorak",
  //              keyboard_layout: "us(dvorak)", language_code: "eng"
  //          }
  const InputMethodDescriptor* GetInputMethodDescriptorFromXkbId(
      const std::string& xkb_id) const;

  // Gets input method IDs that belong to |language_code|.
  // If |type| is |kKeyboardLayoutsOnly|, the function does not return input
  // methods that are not for keybord layout switching. Returns true on success.
  // Note that the function might return false or |language_code| is unknown.
  //
  // The retured input method IDs are sorted by populalirty per
  // chromeos/platform/assets/input_methods/whitelist.txt.
  bool GetInputMethodIdsFromLanguageCode(
      const std::string& language_code,
      InputMethodType type,
      std::vector<std::string>* out_input_method_ids) const;

  // Gets the input method IDs suitable for the first user login, based on
  // the given language code (UI language), and the descriptor of the
  // current input method.
  void GetFirstLoginInputMethodIds(
      const std::string& language_code,
      const InputMethodDescriptor& current_input_method,
      std::vector<std::string>* out_input_method_ids) const;

  // Gets the language codes associated with the given input method IDs.
  // The returned language codes won't have duplicates.
  void GetLanguageCodesFromInputMethodIds(
      const std::vector<std::string>& input_method_ids,
      std::vector<std::string>* out_language_codes) const;

  // Returns the input method ID of the hardware keyboard. e.g. "xkb:us::eng"
  // for the US Qwerty keyboard.
  std::string GetHardwareInputMethodId() const;

  // This function should be called when Chrome's application locale is
  // changed, so that the internal maps of this library is reloaded.
  void OnLocaleChanged();

  // Returns true if the given input method id is supported.
  bool IsValidInputMethodId(const std::string& input_method_id) const;

  // Returns true if the given input method id is for a keyboard layout.
  static bool IsKeyboardLayout(const std::string& input_method_id);

  // Converts a language code to a language display name, using the
  // current application locale. MaybeRewriteLanguageName() is called
  // internally.
  // Examples: "fi"    => "Finnish"
  //           "en-US" => "English (United States)"
  string16 GetLanguageDisplayNameFromCode(
      const std::string& language_code);

  // Converts a language code to a language native display name.
  // MaybeRewriteLanguageName() is called internally.
  // Examples: "fi"    => "suomi" (rather than Finnish)
  //           "en-US" => "English (United States)"
  static string16 GetLanguageNativeDisplayNameFromCode(
      const std::string& language_code);

  // Returns extra language code list associated with |input_method_id|. If
  // there is no associated langauge code, this function returns empty list.
  std::vector<std::string> GetExtraLanguageCodesFromId(
      const std::string& input_method_id) const;

  // Returns all extra language code list.
  std::vector<std::string> GetExtraLanguageCodeList() const;

 protected:
  // This method is ONLY for unit testing. Returns true if the given string is
  // supported (i.e. the string is associated with a resource ID). protected:
  // for testability.
  bool StringIsSupported(const std::string& english_string) const;

  // protected: for unit testing as well.
  bool GetInputMethodIdsFromLanguageCodeInternal(
      const std::multimap<std::string, std::string>& language_code_to_ids,
      const std::string& normalized_language_code,
      InputMethodType type,
      std::vector<std::string>* out_input_method_ids) const;

  // protected: for unit testing as well.
  void ReloadInternalMaps();

  // Sorts the given language codes by their corresponding language names, using
  // the unicode string comparator. Uses unstable sorting. protected: for unit
  // testing as well.
  void SortLanguageCodesByNames(
      std::vector<std::string>* language_codes);

  // All input methods that are supported, including ones not active.
  // protected: for testing.
  scoped_ptr<InputMethodDescriptors> supported_input_methods_;

 private:
  bool TranslateStringInternal(const std::string& english_string,
                               string16 *out_string) const;

  // Map from language code to associated input method IDs, etc.
  typedef std::multimap<std::string, std::string> LanguageCodeToIdsMap;
  // Map from input method ID to associated input method descriptor.
  typedef std::map<
    std::string, InputMethodDescriptor> InputMethodIdToDescriptorMap;
  // Map from XKB layout ID to associated input method descriptor.
  typedef std::map<std::string, InputMethodDescriptor> XkbIdToDescriptorMap;

  LanguageCodeToIdsMap language_code_to_ids_;
  std::map<std::string, std::string> id_to_language_code_;
  InputMethodIdToDescriptorMap id_to_descriptor_;
  XkbIdToDescriptorMap xkb_id_to_descriptor_;

  typedef base::hash_map<std::string, int> HashType;
  HashType english_to_resource_id_;

  InputMethodDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodUtil);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_UTIL_H_
