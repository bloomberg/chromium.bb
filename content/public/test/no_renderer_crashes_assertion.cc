// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/no_renderer_crashes_assertion.h"

#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/notification_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

int g_suspension_count = 0;

}  // namespace

NoRendererCrashesAssertion::NoRendererCrashesAssertion() {
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
}

NoRendererCrashesAssertion::~NoRendererCrashesAssertion() = default;

void NoRendererCrashesAssertion::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (g_suspension_count != 0)
    return;

  if (type != NOTIFICATION_RENDERER_PROCESS_CLOSED)
    return;

  ChildProcessTerminationInfo* process_info =
      content::Details<content::ChildProcessTerminationInfo>(details).ptr();
  switch (process_info->status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return;  // Not a crash.
    default:
      break;  // Crash - need to trigger a test failure below.
  }

  FAIL() << "Unexpected termination of a renderer process";
}

ScopedAllowRendererCrashes::ScopedAllowRendererCrashes() {
  g_suspension_count++;
}

ScopedAllowRendererCrashes::~ScopedAllowRendererCrashes() {
  g_suspension_count--;
}

}  // namespace content
