// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/printing/chrome_print_web_view_helper_delegate.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"
#endif  // defined(ENABLE_EXTENSIONS)

ChromePrintWebViewHelperDelegate::~ChromePrintWebViewHelperDelegate(){
}

bool ChromePrintWebViewHelperDelegate::CancelPrerender(
    content::RenderView* render_view, int routing_id) {
  if (!render_view || !prerender::PrerenderHelper::IsPrerendering(
      render_view->GetMainRenderFrame()))
    return false;

  return render_view->Send(
      new ChromeViewHostMsg_CancelPrerenderForPrinting(routing_id));
}

// Return the PDF object element if |frame| is the out of process PDF extension.
blink::WebElement ChromePrintWebViewHelperDelegate::GetPdfElement(
        blink::WebLocalFrame* frame) {
#if defined(ENABLE_EXTENSIONS)
  GURL url = frame->document().url();
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      url.host() == extension_misc::kPdfExtensionId) {
    // <object> with id="plugin" is created in
    // chrome/browser/resources/pdf/pdf.js.
    auto plugin_element = frame->document().getElementById("plugin");
    if (!plugin_element.isNull()) {
      return plugin_element;
    }
    NOTREACHED();
  }
#endif  // defined(ENABLE_EXTENSIONS)
  return blink::WebElement();
}
