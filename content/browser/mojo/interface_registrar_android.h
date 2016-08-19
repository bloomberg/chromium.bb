// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_INTERFACE_REGISTRAR_ANDROID_H_
#define CONTENT_BROWSER_MOJO_INTERFACE_REGISTRAR_ANDROID_H_

#include <jni.h>

namespace content {

class InterfaceRegistryAndroid;
class RenderFrameHost;

// Registrar for interfaces implemented in Java exposed by the browser. This
// calls into Java where the services are registered with the indicated
// registry.
class InterfaceRegistrarAndroid {
 public:
  static void ExposeInterfacesToRenderer(InterfaceRegistryAndroid* registry);
  static void ExposeInterfacesToFrame(InterfaceRegistryAndroid* registry,
      RenderFrameHost* frame);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_INTERFACE_REGISTRAR_ANDROID_H_
