// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/android_webview_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/command_line.h"
#include "base/test/test_suite.h"
#include "content/public/common/content_switches.h"
#include "ui/gl/gl_surface.h"

int main(int argc, char** argv) {
  android_webview::RegisterJni(base::android::AttachCurrentThread());
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kSingleProcess);
  gfx::GLSurface::InitializeOneOffForTests();
  return base::TestSuite(argc, argv).Run();
}
