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
    ui::AXEvent event) {
  switch (event) {
    case ui::AX_EVENT_ACTIVEDESCENDANTCHANGED:
      return "active descendant changed";
    case ui::AX_EVENT_ARIA_ATTRIBUTE_CHANGED:
      return "aria attribute changed";
    case ui::AX_EVENT_AUTOCORRECTION_OCCURED:
      return "autocorrection occurred";
    case ui::AX_EVENT_BLUR:
      return "blur";
    case ui::AX_EVENT_ALERT:
      return "alert";
    case ui::AX_EVENT_CHECKED_STATE_CHANGED:
      return "check state changed";
    case ui::AX_EVENT_CHILDREN_CHANGED:
      return "children changed";
    case ui::AX_EVENT_FOCUS:
      return "focus changed";
    case ui::AX_EVENT_INVALID_STATUS_CHANGED:
      return "invalid status changed";
    case ui::AX_EVENT_LAYOUT_COMPLETE:
      return "layout complete";
    case ui::AX_EVENT_LIVE_REGION_CHANGED:
      return "live region changed";
    case ui::AX_EVENT_LOAD_COMPLETE:
      return "load complete";
    case ui::AX_EVENT_MENU_LIST_ITEM_SELECTED:
      return "menu list item selected";
    case ui::AX_EVENT_MENU_LIST_VALUE_CHANGED:
      return "menu list changed";
    case ui::AX_EVENT_SHOW:
      return "object show";
    case ui::AX_EVENT_HIDE:
      return "object hide";
    case ui::AX_EVENT_ROW_COUNT_CHANGED:
      return "row count changed";
    case ui::AX_EVENT_ROW_COLLAPSED:
      return "row collapsed";
    case ui::AX_EVENT_ROW_EXPANDED:
      return "row expanded";
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      return "scrolled to anchor";
    case ui::AX_EVENT_SELECTED_CHILDREN_CHANGED:
      return "selected children changed";
    case ui::AX_EVENT_SELECTED_TEXT_CHANGED:
      return "selected text changed";
    case ui::AX_EVENT_TEXT_CHANGED:
      return "text changed";
    case ui::AX_EVENT_TEXT_INSERTED:
      return "text inserted";
    case ui::AX_EVENT_TEXT_REMOVED:
      return "text removed";
    case ui::AX_EVENT_VALUE_CHANGED:
      return "value changed";
    default:
      NOTREACHED();
  }
  return "";
}
#endif

}  // namespace content
