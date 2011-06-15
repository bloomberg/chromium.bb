// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_VIRTUAL_KEYBOARD_SELECTOR_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_VIRTUAL_KEYBOARD_SELECTOR_H_
#pragma once

#include <list>
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
  // keyboard extensions.
  // TODO(yusukes): Add RemoveVirtualKeyboard() as well.
  void AddVirtualKeyboard(const GURL& url,
                          const std::set<std::string>& supported_layouts,
                          bool is_system);

  // Selects and returns the most suitable virtual keyboard extension for the
  // |layout|. Returns NULL if no virtual keyboard extension for the layout
  // is found. If |current_|, which is the virtual keyboard extension currently
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

  // TODO(yusukes): Add a function something like
  //   void SetUserPreference(const std::string& layout,
  //                          const VirtualKeyboard& keyboard);
  // so that users could use a specific virtual keyboard extension for the
  // |layout|.

 protected:
  // This function neither checks |current_| nor updates the variable. The
  // function is protected for testability.
  const VirtualKeyboard* SelectVirtualKeyboardInternal(
      const std::string& layout);

 private:
  // A list of third party virtual keyboard extensions.
  std::list<const VirtualKeyboard*> keyboards_;
  // A list of system virtual keyboard extensions.
  std::list<const VirtualKeyboard*> system_keyboards_;

  // The virtual keyboard currently in use.
  const VirtualKeyboard* current_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardSelector);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_VIRTUAL_KEYBOARD_SELECTOR_H_
