// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_VIRTUAL_KEYBOARD_SELECTOR_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_VIRTUAL_KEYBOARD_SELECTOR_H_
#pragma once

#include <list>
#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

namespace chromeos {
namespace input_method {

// A class which represents a virtual keyboard extension. One virtual keyboard
// extension can support more than one keyboard layout.
class VirtualKeyboard {
 public:
  VirtualKeyboard(const GURL& url,
                  const std::set<std::string>& supported_layouts,
                  bool is_system);
  ~VirtualKeyboard();

  // Returns URL for displaying the keyboard UI specified by |layout|.
  // For example, when |url_| is "http://adcfj..kjhil/" and |layout| is "us",
  // the function would return "http://adcfj..kjhil/index.html#us". When
  // |layout| is empty, it returns |url_| as-is, which is "http://adcfj..kjhil/"
  // in this case.
  GURL GetURLForLayout(const std::string& layout) const;

  // Returns true if the virtual keyboard extension supports the |layout|.
  bool IsLayoutSupported(const std::string& layout) const;

  const GURL& url() const { return url_; }
  const std::set<std::string>& supported_layouts() const {
    return supported_layouts_;
  }
  bool is_system() const { return is_system_; }

 private:
  const GURL url_;
  const std::set<std::string> supported_layouts_;
  const bool is_system_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboard);
};

// A class which holds all available virtual keyboard extensions.
class VirtualKeyboardSelector {
 public:
  VirtualKeyboardSelector();
  ~VirtualKeyboardSelector();

  // Adds a new virtual keyboard extension. If |keyboard.is_system_| is true,
  // the virtual keyboard extension will have lower priority than non-system
  // keyboard extensions. Returns false if a virtual keyboard extension
  // specified by the |url| is already added.
  // TODO(yusukes): Add RemoveVirtualKeyboard() as well.
  bool AddVirtualKeyboard(const GURL& url,
                          const std::set<std::string>& supported_layouts,
                          bool is_system);

  // Selects and returns the most suitable virtual keyboard extension for the
  // |layout|. Returns NULL if no virtual keyboard extension for the layout
  // is found. If a specific virtual keyboard extension for the |layout| is
  // already set by SetUserPreference, the virtual keyboard extension is always
  // returned. If |current_|, which is the virtual keyboard extension currently
  // in use, supports the |layout|, the current one will be returned. Otherwise
  // the function scans the list of |keyboards_| and then the list of
  // |system_keyboards_|. The most recently added keyboards to each list take
  // precedence.
  //
  // Checking the |current_| keyboard is important for the following use case:
  // - If I have installed a VK extension that provides a US and an FR layout
  //   and I switch from the US layout of the extension (+ English IME) to the
  //   French IME, then I would like to use the FR layout of the extension I am
  //   currently using.
  const VirtualKeyboard* SelectVirtualKeyboard(const std::string& layout);

  // Sets user preferences on virtual keyboard selection so that the virtual
  // keyboard extension specified by the |url| is always selected for the
  // |layout|. Returns false if a virtual keyboard extension whose address is
  // |url| is not registered, or the extension specified by the |url| does not
  // support the |layout|.
  bool SetUserPreference(const std::string& layout, const GURL& url);

  // Removes the preference for the |layout| added by SetUserPreference.
  void RemoveUserPreference(const std::string& layout);

 protected:
  // Selects and returns the most suitable virtual keyboard extension for the
  // |layout|. Unlike SelectVirtualKeyboard(), this function only scans
  // |keyboards_| and |system_keyboards_| (in this order), and never updates
  // |current_|. The function is protected for testability.
  const VirtualKeyboard* SelectVirtualKeyboardWithoutPreferences(
      const std::string& layout);

  // The function is protected for testability.
  const std::map<std::string, const VirtualKeyboard*>& user_preference() const {
    return user_preference_;
  }

 private:
  // A list of third party virtual keyboard extensions.
  std::list<const VirtualKeyboard*> keyboards_;
  // A list of system virtual keyboard extensions.
  std::list<const VirtualKeyboard*> system_keyboards_;

  // A map from layout name to virtual keyboard extension.
  std::map<std::string, const VirtualKeyboard*> user_preference_;

  // TODO(yusukes): Support per-site preference. e.g. always use virtual
  // keyboard ABC on https://mail.google.com/, XYZ on http://www.google.com/.

  // The virtual keyboard currently in use.
  const VirtualKeyboard* current_;

  // A map from URL to virtual keyboard extension. The map is for making
  // SetUserPreference() faster.
  std::map<GURL, const VirtualKeyboard*> url_to_keyboard_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardSelector);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_VIRTUAL_KEYBOARD_SELECTOR_H_
