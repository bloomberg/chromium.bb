// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_event_router_base.h"

#include "base/strings/string_number_conversions.h"

namespace extensions {

InputImeEventRouterBase::InputImeEventRouterBase(Profile* profile)
    : next_request_id_(1), profile_(profile) {}

InputImeEventRouterBase::~InputImeEventRouterBase() {}

void InputImeEventRouterBase::OnKeyEventHandled(const std::string& extension_id,
                                                const std::string& request_id,
                                                bool handled) {
  RequestMap::iterator request = request_map_.find(request_id);
  if (request == request_map_.end()) {
    LOG(ERROR) << "Request ID not found: " << request_id;
    return;
  }

  request->second.second.Run(handled);
  request_map_.erase(request);
}

std::string InputImeEventRouterBase::AddRequest(
    const std::string& component_id,
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback& key_data) {
  std::string request_id = base::IntToString(next_request_id_);
  ++next_request_id_;

  request_map_[request_id] = std::make_pair(component_id, key_data);

  return request_id;
}

}  // namespace extensions
