// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/renderer_accessibility.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebAccessibilityNotification;
using WebKit::WebAccessibilityObject;
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
const std::string RendererAccessibility::AccessibilityNotificationToString(
    AccessibilityNotification notification) {
  switch (notification) {
    case AccessibilityNotificationActiveDescendantChanged:
      return "active descendant changed";
    case AccessibilityNotificationAriaAttributeChanged:
      return "aria attribute changed";
    case AccessibilityNotificationAutocorrectionOccurred:
      return "autocorrection occurred";
    case AccessibilityNotificationBlur:
      return "blur";
    case AccessibilityNotificationAlert:
      return "alert";
    case AccessibilityNotificationCheckStateChanged:
      return "check state changed";
    case AccessibilityNotificationChildrenChanged:
      return "children changed";
    case AccessibilityNotificationFocusChanged:
      return "focus changed";
    case AccessibilityNotificationInvalidStatusChanged:
      return "invalid status changed";
    case AccessibilityNotificationLayoutComplete:
      return "layout complete";
    case AccessibilityNotificationLiveRegionChanged:
      return "live region changed";
    case AccessibilityNotificationLoadComplete:
      return "load complete";
    case AccessibilityNotificationMenuListItemSelected:
      return "menu list item selected";
    case AccessibilityNotificationMenuListValueChanged:
      return "menu list changed";
    case AccessibilityNotificationObjectShow:
      return "object show";
    case AccessibilityNotificationObjectHide:
      return "object hide";
    case AccessibilityNotificationRowCountChanged:
      return "row count changed";
    case AccessibilityNotificationRowCollapsed:
      return "row collapsed";
    case AccessibilityNotificationRowExpanded:
      return "row expanded";
    case AccessibilityNotificationScrolledToAnchor:
      return "scrolled to anchor";
    case AccessibilityNotificationSelectedChildrenChanged:
      return "selected children changed";
    case AccessibilityNotificationSelectedTextChanged:
      return "selected text changed";
    case AccessibilityNotificationTextChanged:
      return "text changed";
    case AccessibilityNotificationTextInserted:
      return "text inserted";
    case AccessibilityNotificationTextRemoved:
      return "text removed";
    case AccessibilityNotificationValueChanged:
      return "value changed";
    default:
      NOTREACHED();
  }
  return "";
}
#endif

}  // namespace content
