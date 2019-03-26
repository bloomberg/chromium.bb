// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/profile_builder.h"

namespace base {

ProfileBuilder::Frame::Frame(uintptr_t instruction_pointer,
                             const ModuleCache::Module* module)
    : instruction_pointer(instruction_pointer), module(module) {}

ProfileBuilder::Frame::~Frame() = default;

}  // namespace base
