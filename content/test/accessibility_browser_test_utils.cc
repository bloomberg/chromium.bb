// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/accessibility_browser_test_utils.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

AccessibilityNotificationWaiter::AccessibilityNotificationWaiter(
    Shell* shell,
    AccessibilityMode accessibility_mode,
    blink::WebAXEvent event_type)
    : shell_(shell),
      event_to_wait_for_(event_type),
      loop_runner_(new MessageLoopRunner()),
      weak_factory_(this) {
  WebContents* web_contents = shell_->web_contents();
  view_host_ = static_cast<RenderViewHostImpl*>(
      web_contents->GetRenderViewHost());
  view_host_->SetAccessibilityCallbackForTesting(
      base::Bind(&AccessibilityNotificationWaiter::OnAccessibilityEvent,
                 weak_factory_.GetWeakPtr()));
  view_host_->SetAccessibilityMode(accessibility_mode);
}

AccessibilityNotificationWaiter::~AccessibilityNotificationWaiter() {
}

void AccessibilityNotificationWaiter::WaitForNotification() {
  loop_runner_->Run();
}

const AccessibilityNodeDataTreeNode&
AccessibilityNotificationWaiter::GetAccessibilityNodeDataTree() const {
  return view_host_->accessibility_tree_for_testing();
}

void AccessibilityNotificationWaiter::OnAccessibilityEvent(
    blink::WebAXEvent event_type) {
  if (!IsAboutBlank() && event_to_wait_for_ == event_type)
    loop_runner_->Quit();
}

bool AccessibilityNotificationWaiter::IsAboutBlank() {
  // Skip any accessibility notifications related to "about:blank",
  // to avoid a possible race condition between the test beginning
  // listening for accessibility events and "about:blank" loading.
  const AccessibilityNodeDataTreeNode& root = GetAccessibilityNodeDataTree();
  for (size_t i = 0; i < root.string_attributes.size(); ++i) {
    if (root.string_attributes[i].first != AccessibilityNodeData::ATTR_DOC_URL)
      continue;
    const std::string& doc_url = root.string_attributes[i].second;
    return doc_url == kAboutBlankURL;
  }
  return false;
}

}  // namespace content
