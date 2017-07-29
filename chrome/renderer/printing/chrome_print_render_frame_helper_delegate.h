// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRINTING_CHROME_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
#define CHROME_RENDERER_PRINTING_CHROME_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_

#include "components/printing/renderer/print_render_frame_helper.h"

class ChromePrintRenderFrameHelperDelegate
    : public printing::PrintRenderFrameHelper::Delegate {
 public:
  ~ChromePrintRenderFrameHelperDelegate() override;

  bool CancelPrerender(content::RenderFrame* render_frame) override;

  blink::WebElement GetPdfElement(blink::WebLocalFrame* frame) override;

  bool IsPrintPreviewEnabled() override;

  bool OverridePrint(blink::WebLocalFrame* frame) override;
};  // class ChromePrintRenderFrameHelperDelegate

#endif  // CHROME_RENDERER_PRINTING_CHROME_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
