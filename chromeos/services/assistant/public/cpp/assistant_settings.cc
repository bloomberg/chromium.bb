// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/cpp/assistant_settings.h"

namespace chromeos {
namespace assistant {

namespace {

AssistantSettings* g_instance = nullptr;

}  // namespace

// static
AssistantSettings* AssistantSettings::Get() {
  return g_instance;
}

AssistantSettings::AssistantSettings() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AssistantSettings::~AssistantSettings() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace assistant
}  // namespace chromeos
