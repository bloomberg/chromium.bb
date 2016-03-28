// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/engine_settings.h"

namespace blimp {
namespace engine {

EngineSettings::EngineSettings() : record_whole_document(false) {}

EngineSettings::~EngineSettings() {}

EngineSettings::EngineSettings(const EngineSettings& other) = default;

}  // namespace engine
}  // namespace blimp
