// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/mojo_application_info.h"

#include "base/callback.h"
#include "base/single_thread_task_runner.h"

namespace content {

MojoApplicationInfo::MojoApplicationInfo() {}

MojoApplicationInfo::MojoApplicationInfo(const MojoApplicationInfo& other) {
  application_factory = other.application_factory;
  application_task_runner = other.application_task_runner;
}

MojoApplicationInfo::~MojoApplicationInfo() {}

}  // namespace content
