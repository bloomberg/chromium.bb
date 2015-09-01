// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/courgette_config.h"

#include "base/command_line.h"
#include "courgette/ensemble.h"

namespace courgette {

namespace {

static const uint32 kExperimentalVersion = 0xDEADBEEF;

}  // namespace

// static
CourgetteConfig* CourgetteConfig::GetInstance() {
  return Singleton<CourgetteConfig>::get();
}

uint32 CourgetteConfig::ensemble_version() const {
  return is_experimental_ ? kExperimentalVersion : CourgettePatchFile::kVersion;
}

void CourgetteConfig::Initialize(const base::CommandLine& command_line) {
  is_experimental_ = command_line.HasSwitch("experimental");
}

CourgetteConfig::CourgetteConfig() : is_experimental_(false) {
}

}  // namespace courgette
