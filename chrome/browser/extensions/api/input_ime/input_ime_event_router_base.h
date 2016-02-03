// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_EVENT_ROUTER_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_EVENT_ROUTER_BASE_H_

#include <map>
#include <string>
#include <utility>

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/input_method/input_method_engine_base.h"
#include "ui/base/ime/ime_engine_handler_interface.h"

namespace extensions {

class InputImeEventRouterBase {
 public:
  explicit InputImeEventRouterBase(Profile* profile);
  virtual ~InputImeEventRouterBase();

  // Gets the input method engine if the extension is active.
  virtual input_method::InputMethodEngineBase* GetActiveEngine(
      const std::string& extension_id) = 0;

  // Called when a key event was handled.
  void OnKeyEventHandled(const std::string& extension_id,
                         const std::string& request_id,
                         bool handled);

  std::string AddRequest(
      const std::string& component_id,
      ui::IMEEngineHandlerInterface::KeyEventDoneCallback& key_data);

  Profile* profile() const { return profile_; }

 private:
  using RequestMap =
      std::map<std::string,
               std::pair<std::string,
                         ui::IMEEngineHandlerInterface::KeyEventDoneCallback>>;

  unsigned int next_request_id_;
  RequestMap request_map_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(InputImeEventRouterBase);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_EVENT_ROUTER_BASE_H_
