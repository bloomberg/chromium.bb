// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/headless_content_renderer_client.h"

#include "base/memory/ptr_util.h"
#include "headless/lib/renderer/headless_render_frame_controller_impl.h"
#include "printing/features/features.h"

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
#include "components/printing/renderer/print_web_view_helper.h"
#include "headless/lib/renderer/headless_print_web_view_helper_delegate.h"
#endif

namespace headless {

HeadlessContentRendererClient::HeadlessContentRendererClient() {}

HeadlessContentRendererClient::~HeadlessContentRendererClient() {}

void HeadlessContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_BASIC_PRINTING)
  new printing::PrintWebViewHelper(
      render_frame, base::MakeUnique<HeadlessPrintWebViewHelperDelegate>());
#endif
  new HeadlessRenderFrameControllerImpl(render_frame);
}

}  // namespace headless
