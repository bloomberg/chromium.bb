// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_ACCESSIBILITY_CONTROLLER_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_ACCESSIBILITY_CONTROLLER_H_

#include <vector>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/shell/renderer/web_test/web_ax_object_proxy.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/ax_event_intent.h"
#include "v8/include/v8.h"

namespace blink {

class WebAXContext;
class WebLocalFrame;
class WebString;
class WebView;

}  // namespace blink

namespace content {

class WebViewTestProxy;

class AccessibilityController {
 public:
  explicit AccessibilityController(WebViewTestProxy* web_view_test_proxy);
  ~AccessibilityController();

  void Reset();
  void Install(blink::WebLocalFrame* frame);
  bool ShouldLogAccessibilityEvents();
  void NotificationReceived(
      blink::WebLocalFrame* frame,
      const blink::WebAXObject& target,
      const std::string& notification_name,
      const std::vector<ui::AXEventIntent>& event_intents);
  void PostNotification(const blink::WebAXObject& target,
                        const std::string& notification_name,
                        const std::vector<ui::AXEventIntent>& event_intents);

 private:
  friend class AccessibilityControllerBindings;

  // Bound methods and properties
  void LogAccessibilityEvents();
  void SetNotificationListener(v8::Local<v8::Function> callback);
  void UnsetNotificationListener();
  v8::Local<v8::Object> FocusedElement();
  v8::Local<v8::Object> RootElement();
  v8::Local<v8::Object> AccessibleElementById(const std::string& id);

  v8::Local<v8::Object> FindAccessibleElementByIdRecursive(
      const blink::WebAXObject&,
      const blink::WebString& id);

  blink::WebAXObject GetAccessibilityObjectForMainFrame();

  // If true, will log all accessibility notifications.
  bool log_accessibility_events_;

  WebAXObjectProxyList elements_;

  v8::Persistent<v8::Function> notification_callback_;

  blink::WebView* web_view();
  WebViewTestProxy* web_view_test_proxy_;

  std::unique_ptr<blink::WebAXContext> ax_context_;

  base::WeakPtrFactory<AccessibilityController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AccessibilityController);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_ACCESSIBILITY_CONTROLLER_H_
