// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSIONS_RENDER_FRAME_OBSERVER_H_
#define EXTENSIONS_RENDERER_EXTENSIONS_RENDER_FRAME_OBSERVER_H_

#include "base/basictypes.h"
#include "content/public/renderer/render_frame_observer.h"

namespace extensions {

// This class holds the extensions specific parts of RenderFrame, and has the
// same lifetime.
class ExtensionsRenderFrameObserver
    : public content::RenderFrameObserver {
 public:
  explicit ExtensionsRenderFrameObserver(
      content::RenderFrame* render_frame);
  virtual ~ExtensionsRenderFrameObserver();

 private:
  // RenderFrameObserver implementation.
  virtual void DetailedConsoleMessageAdded(const base::string16& message,
                                           const base::string16& source,
                                           const base::string16& stack_trace,
                                           int32 line_number,
                                           int32 severity_level) OVERRIDE;
  virtual void DidChangeName(const base::string16& name) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsRenderFrameObserver);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSIONS_RENDER_FRAME_OBSERVER_H_

