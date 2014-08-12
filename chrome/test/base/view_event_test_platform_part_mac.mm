// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/view_event_test_platform_part.h"

namespace {

// ViewEventTestPlatformPart implementation for toolkit-views on Mac.
class ViewEventTestPlatformPartMac : public ViewEventTestPlatformPart {
 public:
  ViewEventTestPlatformPartMac() {}

  // Overridden from ViewEventTestPlatformPart:
  virtual gfx::NativeWindow GetContext() OVERRIDE { return NULL; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewEventTestPlatformPartMac);
};

}  // namespace

// static
ViewEventTestPlatformPart* ViewEventTestPlatformPart::Create(
    ui::ContextFactory* context_factory) {
  return new ViewEventTestPlatformPartMac();
}
