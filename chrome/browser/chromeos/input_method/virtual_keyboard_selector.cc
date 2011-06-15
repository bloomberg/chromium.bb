// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/virtual_keyboard_selector.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"

namespace {
const char kDefaultURLPath[] = "index.html";
const size_t kDefaultURLPathLen = arraysize(kDefaultURLPath) - 1;
}  // namespace

namespace chromeos {
namespace input_method {

VirtualKeyboard::VirtualKeyboard(const GURL& url,
                                 const std::set<std::string>& supported_layouts,
                                 bool is_system)
    : url_(url),
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

VirtualKeyboardSelector::VirtualKeyboardSelector()
    : current_(NULL) {
}

VirtualKeyboardSelector::~VirtualKeyboardSelector() {
  STLDeleteElements(&keyboards_);
}

void VirtualKeyboardSelector::AddVirtualKeyboard(
    const GURL& url,
    const std::set<std::string>& supported_layouts,
    bool is_system) {
  const VirtualKeyboard* new_keyboard = new VirtualKeyboard(url,
                                                            supported_layouts,
                                                            is_system);
  if (is_system) {
    system_keyboards_.push_front(new_keyboard);
  } else {
    keyboards_.push_front(new_keyboard);
  }
}

const VirtualKeyboard* VirtualKeyboardSelector::SelectVirtualKeyboard(
    const std::string& layout) {
  if (layout.empty()) {
    LOG(ERROR) << "No layout is specified";
    return NULL;
  }

  // First, check whether the current keyboard supports the layout.
  if (current_ && current_->supported_layouts().count(layout) > 0) {
    return current_;
  }

  const VirtualKeyboard* keyboard = SelectVirtualKeyboardInternal(layout);
  if (!keyboard) {
    VLOG(1) << "No virtual keyboard for " << layout << " is found";
    return NULL;
  }

  current_ = keyboard;
  return keyboard;
}

const VirtualKeyboard* VirtualKeyboardSelector::SelectVirtualKeyboardInternal(
    const std::string& layout) {
  std::list<const VirtualKeyboard*>::const_iterator iter;
  for (iter = keyboards_.begin(); iter != keyboards_.end(); ++iter) {
    if ((*iter)->supported_layouts().count(layout) > 0) {
      return *iter;
    }
  }
  for (iter = system_keyboards_.begin();
       iter != system_keyboards_.end(); ++iter) {
    if ((*iter)->supported_layouts().count(layout) > 0) {
      return *iter;
    }
  }
  return NULL;
}

}  // namespace input_method
}  // namespace chromeos
