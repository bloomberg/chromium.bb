// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDER_FRAME_OBSERVER_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDER_FRAME_OBSERVER_H_

#include "base/basictypes.h"
#include "content/public/renderer/render_frame_observer.h"

namespace extensions {

// This class holds the extensions specific parts of RenderFrame, and has the
// same lifetime.
class ChromeExtensionsRenderFrameObserver
    : public content::RenderFrameObserver {
 public:
  explicit ChromeExtensionsRenderFrameObserver(
      content::RenderFrame* render_frame);
  virtual ~ChromeExtensionsRenderFrameObserver();

 private:
  // RenderFrameObserver implementation.
  virtual void DetailedConsoleMessageAdded(const base::string16& message,
                                           const base::string16& source,
                                           const base::string16& stack_trace,
                                           int32 line_number,
                                           int32 severity_level) OVERRIDE;
  virtual void DidChangeName(const base::string16& name) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionsRenderFrameObserver);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDER_FRAME_OBSERVER_H_

