// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_VIRTUAL_KEYBOARD_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_VIRTUAL_KEYBOARD_MANAGER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "chrome/browser/ui/webui/options/options_ui.h"
#include "googleurl/src/gurl.h"

namespace base {
class DictionaryValue;
class ListValue;
}  // namespace base

namespace chromeos {

namespace input_method {
class VirtualKeyboard;
}  // namespace input_method;

// A class which provides information to virtual_keyboard.js.
class VirtualKeyboardManagerHandler : public OptionsPageUIHandler {
 public:
  VirtualKeyboardManagerHandler();
  virtual ~VirtualKeyboardManagerHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

 protected:
  typedef std::multimap<
   std::string, const input_method::VirtualKeyboard*> LayoutToKeyboard;
  typedef std::map<GURL, const input_method::VirtualKeyboard*> UrlToKeyboard;

  // Returns true if |layout_to_keyboard| contains |layout| as a key, and the
  // value for |layout| contains |url|. This function is protected for
  // testability.
  static bool ValidateUrl(const UrlToKeyboard& url_to_keyboard,
                          const std::string& layout,
                          const std::string& url);

  // Builds a list from |layout_to_keyboard| and |virtual_keyboard_user_pref|.
  // See virtual_keyboard_list.js for an example of the format the list should
  // take. This function is protected for testability.
  static base::ListValue* CreateVirtualKeyboardList(
      const LayoutToKeyboard& layout_to_keyboard,
      const UrlToKeyboard& url_to_keyboard,
      const base::DictionaryValue* virtual_keyboard_user_pref);

 private:
  // Reads user pref and create a list using CreateVirtualKeyboardList().
  base::ListValue* GetVirtualKeyboardList();

  // Handles chrome.send("updateVirtualKeyboardList") JS call.
  // TODO(yusukes): This function should also be called when user pref is
  // updated by chrome://settings page in other tab.
  void UpdateVirtualKeyboardList(const base::ListValue* args);

  // Handles chrome.send("setVirtualKeyboardPreference") JS call.
  void SetVirtualKeyboardPreference(const base::ListValue* args);
  // Handles chrome.send("clearVirtualKeyboardPreference") JS call.
  void ClearVirtualKeyboardPreference(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardManagerHandler);
};

}  // namespace chromeos

#endif  //  CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_VIRTUAL_KEYBOARD_MANAGER_H_
