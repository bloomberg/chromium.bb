// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_test_launcher_delegate.h"
#include "content/public/test/test_launcher.h"

int main(int argc, char** argv) {
  content::ContentTestLauncherDelegate launcher_delegate;
  return LaunchTests(&launcher_delegate, argc, argv);
}
