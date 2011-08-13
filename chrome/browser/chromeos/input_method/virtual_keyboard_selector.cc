// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/virtual_keyboard_selector.h"

#include "base/logging.h"
#include "base/stl_util.h"

namespace {

const char kDefaultURLPath[] = "index.html";
const size_t kDefaultURLPathLen = arraysize(kDefaultURLPath) - 1;

namespace ime = ::chromeos::input_method;

// Selects and returns a virtual keyboard extension from |keyboards| which
// supports the |layout|.
const ime::VirtualKeyboard* SelectVirtualKeyboardInternal(
    const std::list<const ime::VirtualKeyboard*>& keyboards,
    const std::string& layout) {
  for (std::list<const ime::VirtualKeyboard*>::const_iterator iter =
           keyboards.begin(); iter != keyboards.end(); ++iter) {
    const ime::VirtualKeyboard* keyboard = *iter;
    if (keyboard->IsLayoutSupported(layout))
      return keyboard;
  }
  return NULL;
}

}  // namespace

namespace chromeos {
namespace input_method {

VirtualKeyboard::VirtualKeyboard(const GURL& url,
                                 const std::string& name,
                                 const std::set<std::string>& supported_layouts,
                                 bool is_system)
    : url_(url),
      name_(name),
      supported_layouts_(supported_layouts),
      is_system_(is_system) {
}

VirtualKeyboard::~VirtualKeyboard() {
}

GURL VirtualKeyboard::GetURLForLayout(const std::string& layout) const {
  if (layout.empty()) {
    return url_;
  }
  url_canon::Replacements<char> replacements;
  replacements.SetPath(
      kDefaultURLPath, url_parse::Component(0, kDefaultURLPathLen));
  // TODO(yusukes): would be better to URL-encode the |layout|?
  replacements.SetRef(layout.c_str(), url_parse::Component(0, layout.length()));
  return url_.ReplaceComponents(replacements);
}

bool VirtualKeyboard::IsLayoutSupported(const std::string& layout) const {
  return supported_layouts_.count(layout) > 0;
}

VirtualKeyboardSelector::VirtualKeyboardSelector()
    : current_(NULL) {
}

VirtualKeyboardSelector::~VirtualKeyboardSelector() {
  STLDeleteElements(&keyboards_);
  STLDeleteElements(&system_keyboards_);
}

bool VirtualKeyboardSelector::AddVirtualKeyboard(
    const GURL& url,
    const std::string& name,
    const std::set<std::string>& supported_layouts,
    bool is_system) {
  if (url_to_keyboard_.count(url))
    return false;  // the URL is already in use.

  const VirtualKeyboard* new_keyboard = new VirtualKeyboard(url,
                                                            name,
                                                            supported_layouts,
                                                            is_system);
  if (is_system) {
    system_keyboards_.push_front(new_keyboard);
  } else {
    keyboards_.push_front(new_keyboard);
  }

  url_to_keyboard_.insert(std::make_pair(url, new_keyboard));
  std::set<std::string>::const_iterator layout_iter;
  for (layout_iter = new_keyboard->supported_layouts().begin();
       layout_iter != new_keyboard->supported_layouts().end();
       ++layout_iter) {
    const std::string& layout = *layout_iter;
    layout_to_keyboard_.insert(std::make_pair(layout, new_keyboard));
  }

  return true;
}

const VirtualKeyboard* VirtualKeyboardSelector::SelectVirtualKeyboard(
    const std::string& layout) {
  if (layout.empty()) {
    LOG(ERROR) << "No layout is specified";
    return NULL;
  }

  // First, check the user pref.
  std::map<std::string, const VirtualKeyboard*>::const_iterator iter =
      user_preference_.find(layout);
  if (iter != user_preference_.end() &&
      iter->second->IsLayoutSupported(layout)) {
    current_ = iter->second;
    return current_;
  }

  // Second, check whether the current keyboard supports the layout.
  if (current_ && current_->IsLayoutSupported(layout)) {
    return current_;
  }

  const VirtualKeyboard* keyboard =
      SelectVirtualKeyboardWithoutPreferences(layout);
  if (!keyboard) {
    VLOG(1) << "No virtual keyboard for " << layout << " is found";
    return NULL;
  }

  current_ = keyboard;
  return keyboard;
}

bool VirtualKeyboardSelector::SetUserPreference(
    const std::string& layout, const GURL& url) {
  std::map<GURL, const VirtualKeyboard*>::const_iterator iter =
      url_to_keyboard_.find(url);
  if (iter == url_to_keyboard_.end()) {
    VLOG(1) << "Can't set user preference: unknown URL";
    return false;
  }

  const VirtualKeyboard* keyboard = iter->second;
  if (!keyboard->IsLayoutSupported(layout)) {
    VLOG(1) << "Requested layout is not supported by requested URL";
    return false;
  }

  RemoveUserPreference(layout);
  user_preference_.insert(std::make_pair(layout, keyboard));
  return true;
}

void VirtualKeyboardSelector::RemoveUserPreference(const std::string& layout) {
  user_preference_.erase(layout);
}

void VirtualKeyboardSelector::ClearAllUserPreferences() {
  user_preference_.clear();
}

const VirtualKeyboard*
VirtualKeyboardSelector::SelectVirtualKeyboardWithoutPreferences(
    const std::string& layout) {
  const VirtualKeyboard* keyboard =
      SelectVirtualKeyboardInternal(keyboards_, layout);
  if (!keyboard)
    keyboard = SelectVirtualKeyboardInternal(system_keyboards_, layout);
  return keyboard;
}

}  // namespace input_method
}  // namespace chromeos
