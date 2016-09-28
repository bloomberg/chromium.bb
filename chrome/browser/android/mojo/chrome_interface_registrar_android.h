// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_MOJO_CHROME_INTERFACE_REGISTRAR_ANDROID_H_
#define CHROME_BROWSER_ANDROID_MOJO_CHROME_INTERFACE_REGISTRAR_ANDROID_H_

#include <jni.h>

namespace content {
class RenderFrameHost;
}

namespace shell {
class InterfaceRegistry;
}

class ChromeInterfaceRegistrarAndroid {
 public:
  static void ExposeInterfacesToFrame(
      shell::InterfaceRegistry* registry,
      content::RenderFrameHost* render_frame_host);

 private:
  ChromeInterfaceRegistrarAndroid() {}
  ~ChromeInterfaceRegistrarAndroid() {}
};

#endif  // CHROME_BROWSER_ANDROID_MOJO_CHROME_INTERFACE_REGISTRAR_ANDROID_H_
