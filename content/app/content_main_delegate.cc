// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/content_main_delegate.h"

#include "base/logging.h"

namespace content {

ContentMainDelegate::~ContentMainDelegate() {
}

bool ContentMainDelegate::BasicStartupComplete(int* exit_code) {
  return false;
}

void ContentMainDelegate::PreSandboxStartup() {
}

void ContentMainDelegate::SandboxInitialized(const std::string& process_type) {
}

int ContentMainDelegate::RunProcess(const std::string& process_type,
                       const MainFunctionParams& main_function_params) {
  NOTREACHED();
  return -1;
}

void ContentMainDelegate::ProcessExiting(const std::string& process_type) {
}

#if defined(OS_MACOSX)
bool ContentMainDelegate::ProcessRegistersWithSystemProcess(
    const std::string& process_type) {
  return false;
}

bool ContentMainDelegate::ShouldSendMachPort(const std::string& process_type) {
  return false;
}

bool ContentMainDelegate::DelaySandboxInitialization(
    const std::string& process_type) {
  return false;
}
#elif defined(OS_POSIX)
ZygoteForkDelegate* ContentMainDelegate::ZygoteStarting() {
  return NULL;
}

void ContentMainDelegate::ZygoteForked() {
}
#endif  // OS_MACOSX

}  // namespace content
