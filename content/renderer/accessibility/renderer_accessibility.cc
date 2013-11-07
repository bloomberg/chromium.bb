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

using blink::WebAXObject;
using blink::WebDocument;
using blink::WebFrame;
using blink::WebView;

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
    blink::WebAXEvent event) {
  switch (event) {
    case blink::WebAXEventActiveDescendantChanged:
      return "active descendant changed";
    case blink::WebAXEventAriaAttributeChanged:
      return "aria attribute changed";
    case blink::WebAXEventAutocorrectionOccured:
      return "autocorrection occurred";
    case blink::WebAXEventBlur:
      return "blur";
    case blink::WebAXEventAlert:
      return "alert";
    case blink::WebAXEventCheckedStateChanged:
      return "check state changed";
    case blink::WebAXEventChildrenChanged:
      return "children changed";
    case blink::WebAXEventFocus:
      return "focus changed";
    case blink::WebAXEventInvalidStatusChanged:
      return "invalid status changed";
    case blink::WebAXEventLayoutComplete:
      return "layout complete";
    case blink::WebAXEventLiveRegionChanged:
      return "live region changed";
    case blink::WebAXEventLoadComplete:
      return "load complete";
    case blink::WebAXEventMenuListItemSelected:
      return "menu list item selected";
    case blink::WebAXEventMenuListValueChanged:
      return "menu list changed";
    case blink::WebAXEventShow:
      return "object show";
    case blink::WebAXEventHide:
      return "object hide";
    case blink::WebAXEventRowCountChanged:
      return "row count changed";
    case blink::WebAXEventRowCollapsed:
      return "row collapsed";
    case blink::WebAXEventRowExpanded:
      return "row expanded";
    case blink::WebAXEventScrolledToAnchor:
      return "scrolled to anchor";
    case blink::WebAXEventSelectedChildrenChanged:
      return "selected children changed";
    case blink::WebAXEventSelectedTextChanged:
      return "selected text changed";
    case blink::WebAXEventTextChanged:
      return "text changed";
    case blink::WebAXEventTextInserted:
      return "text inserted";
    case blink::WebAXEventTextRemoved:
      return "text removed";
    case blink::WebAXEventValueChanged:
      return "value changed";
    default:
      NOTREACHED();
  }
  return "";
}
#endif

}  // namespace content
