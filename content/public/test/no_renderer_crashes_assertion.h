// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NO_RENDERER_CRASHES_ASSERTION_H_
#define CONTENT_PUBLIC_TEST_NO_RENDERER_CRASHES_ASSERTION_H_

#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

namespace content {

// Usually, a test should fail when BrowserTestBase detects a crash in a
// renderer process (see the NoRendererCrashesAssertion helper class below).
// The ScopedAllowRendererCrashes class can be used to temporarily suspend this
// behavior - this is useful in tests that explicitly expect renderer kills or
// crashes.
class ScopedAllowRendererCrashes {
 public:
  ScopedAllowRendererCrashes();
  ~ScopedAllowRendererCrashes();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedAllowRendererCrashes);
};

// Helper that BrowserTestBase can use to start monitoring for renderer crashes
// (triggering a test failure when a renderer crash happens).
//
// TODO(lukasza): https://crbug.com/972220: Actually start using this class,
// by constructing it from BrowserTestBase::ProxyRunTestOnMainThreadLoop
// (before calling PreRunTestOnMainThread).
class NoRendererCrashesAssertion : public NotificationObserver {
 public:
  NoRendererCrashesAssertion();
  ~NoRendererCrashesAssertion() override;

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(NoRendererCrashesAssertion);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NO_RENDERER_CRASHES_ASSERTION_H_
