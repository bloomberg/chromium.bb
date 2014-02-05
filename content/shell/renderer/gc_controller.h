// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_GC_CONTROLLER_H_
#define CONTENT_SHELL_RENDERER_GC_CONTROLLER_H_

#include "base/basictypes.h"
#include "gin/wrappable.h"

namespace blink {
class WebFrame;
}

namespace gin {
class Arguments;
}

namespace content {

class GCController : public gin::Wrappable<GCController> {
 public:
  static gin::WrapperInfo kWrapperInfo;
  static void Install(blink::WebFrame* frame);

 private:
  GCController();
  virtual ~GCController();

  // gin::Wrappable.
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  void Collect(const gin::Arguments& args);
  void CollectAll(const gin::Arguments& args);
  void MinorCollect(const gin::Arguments& args);

  DISALLOW_COPY_AND_ASSIGN(GCController);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_GC_CONTROLLER_H_
