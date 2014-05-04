// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_ACCESSIBILITY_CONTROLLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_ACCESSIBILITY_CONTROLLER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/shell/renderer/test_runner/web_ax_object_proxy.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "v8/include/v8.h"

namespace blink {
class WebFrame;
class WebString;
class WebView;
}

namespace content {

class WebTestDelegate;

class AccessibilityController :
      public base::SupportsWeakPtr<AccessibilityController> {
 public:
  AccessibilityController();
  ~AccessibilityController();

  void Reset();
  void Install(blink::WebFrame* frame);
  void SetFocusedElement(const blink::WebAXObject&);
  bool ShouldLogAccessibilityEvents();
  void NotificationReceived(const blink::WebAXObject& target,
                            const std::string& notification_name);

  void SetDelegate(WebTestDelegate* delegate);
  void SetWebView(blink::WebView* web_view);

 private:
  friend class AccessibilityControllerBindings;

  // Bound methods and properties
  void LogAccessibilityEvents();
  void SetNotificationListener(v8::Handle<v8::Function> callback);
  void UnsetNotificationListener();
  v8::Handle<v8::Object> FocusedElement();
  v8::Handle<v8::Object> RootElement();
  v8::Handle<v8::Object> AccessibleElementById(const std::string& id);

  v8::Handle<v8::Object> FindAccessibleElementByIdRecursive(
      const blink::WebAXObject&, const blink::WebString& id);

  // If true, will log all accessibility notifications.
  bool log_accessibility_events_;

  blink::WebAXObject focused_element_;
  blink::WebAXObject root_element_;

  WebAXObjectProxyList elements_;

  v8::Persistent<v8::Function> notification_callback_;

  WebTestDelegate* delegate_;
  blink::WebView* web_view_;

  base::WeakPtrFactory<AccessibilityController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityController);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_ACCESSIBILITY_CONTROLLER_H_
