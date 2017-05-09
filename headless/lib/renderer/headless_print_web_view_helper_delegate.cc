// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/headless_print_web_view_helper_delegate.h"

#include "third_party/WebKit/public/web/WebElement.h"

namespace headless {

HeadlessPrintWebViewHelperDelegate::HeadlessPrintWebViewHelperDelegate() {}

HeadlessPrintWebViewHelperDelegate::~HeadlessPrintWebViewHelperDelegate() {}

bool HeadlessPrintWebViewHelperDelegate::CancelPrerender(
    content::RenderFrame* render_frame) {
  return false;
}

blink::WebElement HeadlessPrintWebViewHelperDelegate::GetPdfElement(
    blink::WebLocalFrame* frame) {
  return blink::WebElement();
}

bool HeadlessPrintWebViewHelperDelegate::IsPrintPreviewEnabled() {
  return false;
}

bool HeadlessPrintWebViewHelperDelegate::OverridePrint(
    blink::WebLocalFrame* frame) {
  return false;
}

bool HeadlessPrintWebViewHelperDelegate::IsAskPrintSettingsEnabled() {
  return true;
}

#if defined(OS_MACOSX)
bool HeadlessPrintWebViewHelperDelegate::UseSingleMetafile() {
  return true;
}
#endif

}  // namespace headless
