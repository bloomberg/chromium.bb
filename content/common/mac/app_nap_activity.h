// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAC_APP_NAP_ACTIVITY_H
#define CONTENT_COMMON_MAC_APP_NAP_ACTIVITY_H

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

// Can't import scoped_nsobject here, so wrap it.
struct AssertionWrapper;

// A wrapper around the macOS "activity" system, which is required to
// make renderers eligible for AppNap.
//
// When doing work, processes are expected to begin an activity, receiving
// an opaque token called an "assertion". On finishing, they end the activity.
// When a process has no outstanding assertions, it becomes eligible for
// AppNap.
class CONTENT_EXPORT AppNapActivity {
 public:
  AppNapActivity();
  ~AppNapActivity();

  // Because there's no NSApplication in renderers, do some housekeeping
  // to become eligible for App Nap.
  static void InitializeAppNapSupport();

  // Begin an activity and store the provided token.
  void Begin();

  // End the activity represented by |assertion_|.
  void End();

 private:
  // An opaque token provided by the OS on beginning an activity.
  std::unique_ptr<AssertionWrapper> assertion_;

  DISALLOW_COPY_AND_ASSIGN(AppNapActivity);
};

}  // namespace content

#endif  // CONTENT_COMMON_MAC_APP_NAP_ACTIVITY_H
