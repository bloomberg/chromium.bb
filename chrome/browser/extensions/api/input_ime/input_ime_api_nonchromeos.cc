// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is for non-chromeos (win & linux) functions, such as
// chrome.input.ime.activate, chrome.input.ime.createWindow and
// chrome.input.ime.onSelectionChanged.
// TODO(azurewei): May refactor the code structure by using delegate or
// redesign the API to remove this platform-specific file in the future.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/common/chrome_switches.h"

namespace {

bool IsInputImeEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableInputImeAPI);
}

class ImeObserverNonChromeOS : public ui::ImeObserver {
 public:
  ImeObserverNonChromeOS(const std::string& extension_id, Profile* profile)
      : ImeObserver(extension_id, profile) {}

  ~ImeObserverNonChromeOS() override {}

 private:
  // ImeObserver overrides.
  void DispatchEventToExtension(
      extensions::events::HistogramValue histogram_value,
      const std::string& event_name,
      scoped_ptr<base::ListValue> args) override {
    if (!IsInputImeEnabled()) {
      return;
    }

    scoped_ptr<extensions::Event> event(
        new extensions::Event(histogram_value, event_name, std::move(args)));
    event->restrict_to_browser_context = profile_;
    extensions::EventRouter::Get(profile_)
        ->DispatchEventToExtension(extension_id_, std::move(event));
  }

  DISALLOW_COPY_AND_ASSIGN(ImeObserverNonChromeOS);
};

}  // namespace

namespace extensions {

bool InputImeEventRouter::RegisterImeExtension(
    const std::string& extension_id,
    const std::vector<extensions::InputComponentInfo>& input_components) {
  return false;
}

void InputImeEventRouter::UnregisterAllImes(const std::string& extension_id) {}

}  // namespace extensions
