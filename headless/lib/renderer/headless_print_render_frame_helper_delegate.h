// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_RENDERER_HEADLESS_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
#define HEADLESS_LIB_RENDERER_HEADLESS_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_

#include "build/build_config.h"
#include "components/printing/renderer/print_render_frame_helper.h"

namespace headless {

class HeadlessPrintRenderFrameHelperDelegate
    : public printing::PrintRenderFrameHelper::Delegate {
 public:
  HeadlessPrintRenderFrameHelperDelegate();
  ~HeadlessPrintRenderFrameHelperDelegate() override;

  // PrintRenderFrameHelper Delegate implementation.
  bool CancelPrerender(content::RenderFrame* render_frame) override;
  bool IsPrintPreviewEnabled() override;
  bool OverridePrint(blink::WebLocalFrame* frame) override;
  bool IsAskPrintSettingsEnabled() override;
  blink::WebElement GetPdfElement(blink::WebLocalFrame* frame) override;

#if defined(OS_MACOSX)
  bool UseSingleMetafile() override;
#endif
};

}  // namespace headless

#endif  // HEADLESS_LIB_RENDERER_HEADLESS_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
