// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PEPPER_HELPER_H_
#define CHROME_RENDERER_PEPPER_PEPPER_HELPER_H_

#include "base/compiler_specific.h"
#include "content/public/renderer/render_view_observer.h"

namespace chrome {

// This class listens for Pepper creation events from the RenderView and
// attaches the parts required for Chrome-specific plugin support.
class PepperHelper : public content::RenderViewObserver {
 public:
  explicit PepperHelper(content::RenderView* render_view);
  virtual ~PepperHelper();

  // RenderViewObserver.
  virtual void DidCreatePepperPlugin(content::RendererPpapiHost* host) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperHelper);
};

}  // namespace chrome

#endif  // CHROME_RENDERER_PEPPER_PEPPER_HELPER_H_
