// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_WEB_TEST_PROXY_H_
#define COMPONENTS_TEST_RUNNER_WEB_TEST_PROXY_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "components/test_runner/test_runner_export.h"
#include "components/test_runner/web_task.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDOMMessageEvent.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "third_party/WebKit/public/web/WebHistoryCommitType.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"

class SkBitmap;
class SkCanvas;

namespace blink {
class WebDragData;
class WebFileChooserCompletion;
class WebLocalFrame;
class WebSpeechRecognizer;
class WebSpellCheckClient;
class WebString;
class WebView;
class WebWidget;
struct WebFileChooserParams;
struct WebPoint;
struct WebWindowFeatures;
}

namespace test_runner {

class MockCredentialManagerClient;
class MockWebSpeechRecognizer;
class SpellCheckClient;
class TestInterfaces;
class WebTestDelegate;
class WebTestInterfaces;

// WebTestProxyBase is the "brain" of WebTestProxy in the sense that
// WebTestProxy does the bridge between RenderViewImpl and WebTestProxyBase and
// when it requires a behavior to be different from the usual, it will call
// WebTestProxyBase that implements the expected behavior.
// See WebTestProxy class comments for more information.
class TEST_RUNNER_EXPORT WebTestProxyBase {
 public:
  void SetInterfaces(WebTestInterfaces* interfaces);
  void SetDelegate(WebTestDelegate* delegate);
  void set_widget(blink::WebWidget* widget) { web_widget_ = widget; }

  void Reset();

  blink::WebSpellCheckClient* GetSpellCheckClient() const;
  bool RunFileChooser(const blink::WebFileChooserParams& params,
                      blink::WebFileChooserCompletion* completion);
  void ShowValidationMessage(const blink::WebString& main_message,
                             blink::WebTextDirection main_message_hint,
                             const blink::WebString& sub_message,
                             blink::WebTextDirection sub_message_hint);

  std::string DumpBackForwardLists();
  void CapturePixelsForPrinting(
      const base::Callback<void(const SkBitmap&)>& callback);
  void CopyImageAtAndCapturePixels(
      int x, int y, const base::Callback<void(const SkBitmap&)>& callback);
  void CapturePixelsAsync(
      const base::Callback<void(const SkBitmap&)>& callback);

  void LayoutAndPaintAsyncThen(const base::Closure& callback);

  void GetScreenOrientationForTesting(blink::WebScreenInfo&);
  MockWebSpeechRecognizer* GetSpeechRecognizerMock();
  MockCredentialManagerClient* GetCredentialManagerClientMock();

  WebTaskList* mutable_task_list() { return &task_list_; }

  blink::WebView* GetWebView() const;

  void PostSpellCheckEvent(const blink::WebString& event_name);

  bool AnimationScheduled() { return animate_scheduled_; }

 protected:
  WebTestProxyBase();
  ~WebTestProxyBase();

  void ScheduleAnimation();
  void StartDragging(blink::WebLocalFrame* frame,
                     const blink::WebDragData& data,
                     blink::WebDragOperationsMask mask,
                     const blink::WebImage& image,
                     const blink::WebPoint& point);
  void DidChangeContents();
  void DidEndEditing();
  bool CreateView(blink::WebLocalFrame* creator,
                  const blink::WebURLRequest& request,
                  const blink::WebWindowFeatures& features,
                  const blink::WebString& frame_name,
                  blink::WebNavigationPolicy policy,
                  bool suppress_opener);
  void SetStatusText(const blink::WebString& text);
  void PrintPage(blink::WebLocalFrame* frame);
  blink::WebSpeechRecognizer* GetSpeechRecognizer();
  bool RequestPointerLock();
  void RequestPointerUnlock();
  bool IsPointerLocked();
  void DidFocus();
  void SetToolTipText(const blink::WebString& text,
                      blink::WebTextDirection direction);
  void DidChangeLocationWithinPage(blink::WebLocalFrame* frame);
  void ResetInputMethod();

  blink::WebString acceptLanguages();

 private:
  void AnimateNow();
  void DrawSelectionRect(SkCanvas* canvas);
  void DidCapturePixelsAsync(
      const base::Callback<void(const SkBitmap&)>& callback,
      const SkBitmap& bitmap);

  TestInterfaces* test_interfaces_;
  WebTestDelegate* delegate_;
  blink::WebWidget* web_widget_;

  WebTaskList task_list_;

  blink::WebImage drag_image_;

  scoped_ptr<SpellCheckClient> spellcheck_;

  bool animate_scheduled_;

  scoped_ptr<MockCredentialManagerClient> credential_manager_client_;
  scoped_ptr<MockWebSpeechRecognizer> speech_recognizer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebTestProxyBase);
};

// WebTestProxy is used during LayoutTests and always instantiated, at time of
// writing with Base=RenderViewImpl. It does not directly inherit from it for
// layering purposes.
// The intent of that class is to wrap RenderViewImpl for tests purposes in
// order to reduce the amount of test specific code in the production code.
// WebTestProxy is only doing the glue between RenderViewImpl and
// WebTestProxyBase, that means that there is no logic living in this class
// except deciding which base class should be called (could be both).
//
// Examples of usage:
//  * when a fooClient has a mock implementation, WebTestProxy can override the
//    fooClient() call and have WebTestProxyBase return the mock implementation.
//  * when a value needs to be overridden by LayoutTests, WebTestProxy can
//    override RenderViewImpl's getter and call a getter from WebTestProxyBase
//    instead. In addition, WebTestProxyBase will have a public setter that
//    could be called from the TestRunner.
#if defined(OS_WIN)
// WebTestProxy is a diamond-shaped hierarchy, with WebWidgetClient at the root.
// VS warns when we inherit the WebWidgetClient method implementations from
// RenderWidget. It's safe to ignore that warning.
#pragma warning(disable: 4250)
#endif
template <class Base, typename... Args>
class WebTestProxy : public Base, public WebTestProxyBase {
 public:
  explicit WebTestProxy(Args... args) : Base(args...) {}

  // WebWidgetClient implementation.
  blink::WebScreenInfo screenInfo() override {
    blink::WebScreenInfo info = Base::screenInfo();
    WebTestProxyBase::GetScreenOrientationForTesting(info);
    return info;
  }

  // WebViewClient implementation.
  void scheduleAnimation() override { WebTestProxyBase::ScheduleAnimation(); }
  void startDragging(blink::WebLocalFrame* frame,
                     const blink::WebDragData& data,
                     blink::WebDragOperationsMask mask,
                     const blink::WebImage& image,
                     const blink::WebPoint& point) override {
    WebTestProxyBase::StartDragging(frame, data, mask, image, point);
    // Don't forward this call to Base because we don't want to do a real
    // drag-and-drop.
  }
  void didChangeContents() override {
    WebTestProxyBase::DidChangeContents();
    Base::didChangeContents();
  }
  blink::WebView* createView(blink::WebLocalFrame* creator,
                             const blink::WebURLRequest& request,
                             const blink::WebWindowFeatures& features,
                             const blink::WebString& frame_name,
                             blink::WebNavigationPolicy policy,
                             bool suppress_opener) override {
    if (!WebTestProxyBase::CreateView(
            creator, request, features, frame_name, policy, suppress_opener))
      return 0;
    return Base::createView(
        creator, request, features, frame_name, policy, suppress_opener);
  }
  void setStatusText(const blink::WebString& text) override {
    WebTestProxyBase::SetStatusText(text);
    Base::setStatusText(text);
  }
  void printPage(blink::WebLocalFrame* frame) override {
    WebTestProxyBase::PrintPage(frame);
  }
  blink::WebSpeechRecognizer* speechRecognizer() override {
    return WebTestProxyBase::GetSpeechRecognizer();
  }
  bool requestPointerLock() override {
    return WebTestProxyBase::RequestPointerLock();
  }
  void requestPointerUnlock() override {
    WebTestProxyBase::RequestPointerUnlock();
  }
  bool isPointerLocked() override {
    return WebTestProxyBase::IsPointerLocked();
  }
  void didFocus() override {
    WebTestProxyBase::DidFocus();
    Base::didFocus();
  }
  void setToolTipText(const blink::WebString& text,
                      blink::WebTextDirection hint) override {
    WebTestProxyBase::SetToolTipText(text, hint);
    Base::setToolTipText(text, hint);
  }
  void resetInputMethod() override { WebTestProxyBase::ResetInputMethod(); }
  bool runFileChooser(const blink::WebFileChooserParams& params,
                      blink::WebFileChooserCompletion* completion) override {
    return WebTestProxyBase::RunFileChooser(params, completion);
  }
  void showValidationMessage(
      const blink::WebRect& anchor_in_root_view,
      const blink::WebString& main_message,
      blink::WebTextDirection main_message_hint,
      const blink::WebString& sub_message,
      blink::WebTextDirection sub_message_hint) override {
    WebTestProxyBase::ShowValidationMessage(main_message, main_message_hint,
                                            sub_message, sub_message_hint);
  }
  blink::WebString acceptLanguages() override {
    return WebTestProxyBase::acceptLanguages();
  }

 private:
  virtual ~WebTestProxy() {}

  DISALLOW_COPY_AND_ASSIGN(WebTestProxy);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_WEB_TEST_PROXY_H_
