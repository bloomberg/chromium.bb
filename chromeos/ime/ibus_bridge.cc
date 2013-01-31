// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/ibus_bridge.h"

#include "base/logging.h"
#include "base/memory/singleton.h"

namespace chromeos {

IBusBridge::IBusBridge()
  : input_context_handler_(NULL),
    engine_handler_(NULL),
    candidate_window_handler_(NULL),
    panel_handler_(NULL) {
}

// static
IBusBridge* IBusBridge::GetInstance() {
  return Singleton<IBusBridge>::get();
}

}  // namespace chromeos
