// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_MAIN_ATHENA_RENDERER_PDF_HELPER_H_
#define ATHENA_MAIN_ATHENA_RENDERER_PDF_HELPER_H_

#include "content/public/renderer/render_frame_observer.h"

namespace athena {

class AthenaRendererPDFHelper : public content::RenderFrameObserver {
 public:
  explicit AthenaRendererPDFHelper(content::RenderFrame* frame);
  virtual ~AthenaRendererPDFHelper();

 private:
  // RenderFrameObserver:
  virtual void DidCreatePepperPlugin(
      content::RendererPpapiHost* host) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AthenaRendererPDFHelper);
};

}  // namespace athena

#endif  // ATHENA_MAIN_ATHENA_RENDERER_PDF_HELPER_H_
