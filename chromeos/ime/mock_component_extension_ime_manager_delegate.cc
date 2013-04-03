// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/mock_component_extension_ime_manager_delegate.h"

namespace chromeos {
namespace input_method {

MockComponentExtIMEManagerDelegate::MockComponentExtIMEManagerDelegate()
    : load_call_count_(0),
      unload_call_count_(0) {
}

MockComponentExtIMEManagerDelegate::~MockComponentExtIMEManagerDelegate() {
}

std::vector<ComponentExtensionIME>
    MockComponentExtIMEManagerDelegate::ListIME() {
  return ime_list_;
}

bool MockComponentExtIMEManagerDelegate::Load(const std::string& extension_id,
                                              const std::string& manifest,
                                              const base::FilePath& path) {
  last_loaded_extension_id_ = extension_id;
  load_call_count_++;
  return true;
}

bool MockComponentExtIMEManagerDelegate::Unload(const std::string& extension_id,
                                                const base::FilePath& path) {
  unload_call_count_++;
  last_unloaded_extension_id_ = extension_id;
  return false;
}

}  // namespace input_method
}  // namespace chromeos
