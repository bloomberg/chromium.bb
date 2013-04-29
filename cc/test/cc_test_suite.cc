// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/cc_test_suite.h"

#include <string>

#include "base/command_line.h"
#include "base/debug/trace_event_impl.h"
#include "base/message_loop.h"
#include "base/threading/thread_id_name_manager.h"
#include "cc/base/switches.h"
#include "cc/test/paths.h"

namespace cc {

CCTestSuite::CCTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

CCTestSuite::~CCTestSuite() {}

void CCTestSuite::Initialize() {
  base::TestSuite::Initialize();
  RegisterPathProvider();

  if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kCCUnittestsTraceEventsToVLOG)) {
    std::string category_string =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kCCUnittestsTraceEventsToVLOG);

    if (!category_string.size())
      category_string = "cc";

    base::debug::TraceLog::GetInstance()->SetEnabled(
        base::debug::CategoryFilter(category_string),
        base::debug::TraceLog::ECHO_TO_VLOG);
  }

  message_loop_.reset(new base::MessageLoop);

  base::ThreadIdNameManager::GetInstance()->SetName(
      base::PlatformThread::CurrentId(),
      "Main");
}

void CCTestSuite::Shutdown() {
  message_loop_.reset();

  base::TestSuite::Shutdown();
}

}  // namespace cc
