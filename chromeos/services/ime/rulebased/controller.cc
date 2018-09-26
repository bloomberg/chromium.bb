// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/rulebased/controller.h"

#include "chromeos/services/ime/rulebased/rules_data.h"

namespace chromeos {
namespace ime {
namespace rulebased {

Controller::Controller() : process_key_count_(0) {}
Controller::~Controller() {}

// static
bool Controller::IsImeSupported(const std::string& id) {
  return RulesData::IsIdSupported(id);
}

void Controller::Activate(const std::string& id) {
  if (current_id_ != id) {
    Reset();
    current_id_ = id;
    current_data_ = RulesData::GetById(id);
  }
}

void Controller::Reset() {
  // TODO(shuchen): Implement this for the ones with transform rules.
  process_key_count_ = 0;
}

ProcessKeyResult Controller::ProcessKey(const std::string& code,
                                        uint8_t modifier_state) {
  process_key_count_++;

  ProcessKeyResult res;
  if (!current_data_)
    return res;
  if (modifier_state > 7)
    return res;

  const KeyMap* key_map = current_data_->GetKeyMapByModifiers(modifier_state);
  auto it = key_map->find(code);
  if (it == key_map->end())
    return res;

  res.key_handled = true;
  res.commit_text = it->second;
  return res;
}

}  // namespace rulebased
}  // namespace ime
}  // namespace chromeos
