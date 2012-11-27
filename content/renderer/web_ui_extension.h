// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBUI_EXTENSION_H_
#define CONTENT_RENDERER_WEBUI_EXTENSION_H_

#include "base/basictypes.h"

namespace v8 {
class Extension;
}

namespace content {

class WebUIExtension {
 public:
  // Returns the v8::Extension object handling WebUI bindings. Caller takes
  // ownership of returned object.
  static v8::Extension* Get();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WebUIExtension);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBUI_EXTENSION_H_
