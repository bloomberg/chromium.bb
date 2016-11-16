// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_print_web_view_helper_delegate.h"

#include "third_party/WebKit/public/web/WebElement.h"

namespace android_webview {

AwPrintWebViewHelperDelegate::~AwPrintWebViewHelperDelegate() {}

bool AwPrintWebViewHelperDelegate::CancelPrerender(
    content::RenderFrame* render_frame) {
  return false;
}

blink::WebElement AwPrintWebViewHelperDelegate::GetPdfElement(
    blink::WebLocalFrame* frame) {
  return blink::WebElement();
}

bool AwPrintWebViewHelperDelegate::IsPrintPreviewEnabled() {
  return false;
}

bool AwPrintWebViewHelperDelegate::IsAskPrintSettingsEnabled() {
  return false;
}

bool AwPrintWebViewHelperDelegate::IsScriptedPrintEnabled() {
  return false;
}

bool AwPrintWebViewHelperDelegate::OverridePrint(blink::WebLocalFrame* frame) {
  return false;
}

}  // namespace android_webview
