// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/gc_extension.h"

#include "v8/include/v8.h"

const char kGCExtensionName[] = "v8/GCController";

namespace extensions_v8 {

// static
v8::Extension* GCExtension::Get() {
  v8::Extension* extension = new v8::Extension(
      kGCExtensionName,
      "(function () {"
      "   var v8_gc;"
      "   if (gc) v8_gc = gc;"
      "   GCController = new Object();"
      "   GCController.collect ="
      "     function() {if (v8_gc) v8_gc(); };"
      "   GCController.minorCollect ="
      "     function() {if (v8_gc) v8_gc(true); };"
      " })();");
  return extension;
}

}  // namespace extensions_v8
