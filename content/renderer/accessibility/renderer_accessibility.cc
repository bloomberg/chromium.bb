// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/renderer_accessibility.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using WebKit::WebAXObject;
using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebView;

namespace content {

RendererAccessibility::RendererAccessibility(
    RenderViewImpl* render_view)
    : RenderViewObserver(render_view),
      render_view_(render_view),
      logging_(false) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableAccessibilityLogging))
    logging_ = true;
}

RendererAccessibility::~RendererAccessibility() {
}

WebDocument RendererAccessibility::GetMainDocument() {
  WebView* view = render_view()->GetWebView();
  WebFrame* main_frame = view ? view->mainFrame() : NULL;

  if (main_frame)
    return main_frame->document();

  return WebDocument();
}

#ifndef NDEBUG
const std::string RendererAccessibility::AccessibilityEventToString(
    WebKit::WebAXEvent event) {
  switch (event) {
    case WebKit::WebAXEventActiveDescendantChanged:
      return "active descendant changed";
    case WebKit::WebAXEventAriaAttributeChanged:
      return "aria attribute changed";
    case WebKit::WebAXEventAutocorrectionOccured:
      return "autocorrection occurred";
    case WebKit::WebAXEventBlur:
      return "blur";
    case WebKit::WebAXEventAlert:
      return "alert";
    case WebKit::WebAXEventCheckedStateChanged:
      return "check state changed";
    case WebKit::WebAXEventChildrenChanged:
      return "children changed";
    case WebKit::WebAXEventFocus:
      return "focus changed";
    case WebKit::WebAXEventInvalidStatusChanged:
      return "invalid status changed";
    case WebKit::WebAXEventLayoutComplete:
      return "layout complete";
    case WebKit::WebAXEventLiveRegionChanged:
      return "live region changed";
    case WebKit::WebAXEventLoadComplete:
      return "load complete";
    case WebKit::WebAXEventMenuListItemSelected:
      return "menu list item selected";
    case WebKit::WebAXEventMenuListValueChanged:
      return "menu list changed";
    case WebKit::WebAXEventShow:
      return "object show";
    case WebKit::WebAXEventHide:
      return "object hide";
    case WebKit::WebAXEventRowCountChanged:
      return "row count changed";
    case WebKit::WebAXEventRowCollapsed:
      return "row collapsed";
    case WebKit::WebAXEventRowExpanded:
      return "row expanded";
    case WebKit::WebAXEventScrolledToAnchor:
      return "scrolled to anchor";
    case WebKit::WebAXEventSelectedChildrenChanged:
      return "selected children changed";
    case WebKit::WebAXEventSelectedTextChanged:
      return "selected text changed";
    case WebKit::WebAXEventTextChanged:
      return "text changed";
    case WebKit::WebAXEventTextInserted:
      return "text inserted";
    case WebKit::WebAXEventTextRemoved:
      return "text removed";
    case WebKit::WebAXEventValueChanged:
      return "value changed";
    default:
      NOTREACHED();
  }
  return "";
}
#endif

}  // namespace content
