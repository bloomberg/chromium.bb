// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/web_test_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include <cctype>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/test_runner/event_sender.h"
#include "components/test_runner/mock_credential_manager_client.h"
#include "components/test_runner/mock_screen_orientation_client.h"
#include "components/test_runner/mock_web_speech_recognizer.h"
#include "components/test_runner/spell_check_client.h"
#include "components/test_runner/test_common.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/test_plugin.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_test_interfaces.h"
#include "components/test_runner/web_test_runner.h"
#include "third_party/WebKit/public/platform/WebLayoutAndPaintAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWidgetClient.h"

namespace test_runner {

namespace {

class LayoutAndPaintCallback : public blink::WebLayoutAndPaintAsyncCallback {
 public:
  LayoutAndPaintCallback(const base::Closure& callback)
      : callback_(callback), wait_for_popup_(false) {
  }
  virtual ~LayoutAndPaintCallback() {
  }

  void set_wait_for_popup(bool wait) { wait_for_popup_ = wait; }

  // WebLayoutAndPaintAsyncCallback implementation.
  void didLayoutAndPaint() override;

 private:
  base::Closure callback_;
  bool wait_for_popup_;
};

class HostMethodTask : public WebMethodTask<WebTestProxyBase> {
 public:
  typedef void (WebTestProxyBase::*CallbackMethodType)();
  HostMethodTask(WebTestProxyBase* object, CallbackMethodType callback)
      : WebMethodTask<WebTestProxyBase>(object), callback_(callback) {}

  void RunIfValid() override { (object_->*callback_)(); }

 private:
  CallbackMethodType callback_;
};

std::string DumpAllBackForwardLists(TestInterfaces* interfaces,
                                    WebTestDelegate* delegate) {
  std::string result;
  const std::vector<WebTestProxyBase*>& window_list =
      interfaces->GetWindowList();
  for (size_t i = 0; i < window_list.size(); ++i)
    result.append(delegate->DumpHistoryForWindow(window_list.at(i)));
  return result;
}
}

WebTestProxyBase::WebTestProxyBase()
    : test_interfaces_(NULL),
      delegate_(NULL),
      web_widget_(NULL),
      spellcheck_(new SpellCheckClient(this)) {
  Reset();
}

WebTestProxyBase::~WebTestProxyBase() {
  test_interfaces_->WindowClosed(this);
}

void WebTestProxyBase::SetInterfaces(WebTestInterfaces* interfaces) {
  test_interfaces_ = interfaces->GetTestInterfaces();
  test_interfaces_->WindowOpened(this);
}

void WebTestProxyBase::SetDelegate(WebTestDelegate* delegate) {
  delegate_ = delegate;
  spellcheck_->SetDelegate(delegate);
  if (speech_recognizer_.get())
    speech_recognizer_->SetDelegate(delegate);
}

blink::WebView* WebTestProxyBase::GetWebView() const {
  DCHECK(web_widget_);
  // TestRunner does not support popup widgets. So |web_widget|_ is always a
  // WebView.
  return static_cast<blink::WebView*>(web_widget_);
}

void WebTestProxyBase::Reset() {
  animate_scheduled_ = false;
}

blink::WebSpellCheckClient* WebTestProxyBase::GetSpellCheckClient() const {
  return spellcheck_.get();
}

bool WebTestProxyBase::RunFileChooser(
    const blink::WebFileChooserParams& params,
    blink::WebFileChooserCompletion* completion) {
  delegate_->PrintMessage("Mock: Opening a file chooser.\n");
  // FIXME: Add ability to set file names to a file upload control.
  return false;
}

void WebTestProxyBase::ShowValidationMessage(
    const blink::WebString& main_message,
    blink::WebTextDirection main_message_hint,
    const blink::WebString& sub_message,
    blink::WebTextDirection sub_message_hint) {
  base::string16 wrapped_main_text = main_message;
  base::string16 wrapped_sub_text = sub_message;

  if (main_message_hint == blink::WebTextDirectionLeftToRight) {
    wrapped_main_text =
        base::i18n::GetDisplayStringInLTRDirectionality(wrapped_main_text);
  } else if (main_message_hint == blink::WebTextDirectionRightToLeft &&
             !base::i18n::IsRTL()) {
    base::i18n::WrapStringWithRTLFormatting(&wrapped_main_text);
  }

  if (!wrapped_sub_text.empty()) {
    if (sub_message_hint == blink::WebTextDirectionLeftToRight) {
      wrapped_sub_text =
          base::i18n::GetDisplayStringInLTRDirectionality(wrapped_sub_text);
    } else if (sub_message_hint == blink::WebTextDirectionRightToLeft) {
      base::i18n::WrapStringWithRTLFormatting(&wrapped_sub_text);
    }
  }
  delegate_->PrintMessage("ValidationMessageClient: main-message=" +
                          base::UTF16ToUTF8(wrapped_main_text) +
                          " sub-message=" +
                          base::UTF16ToUTF8(wrapped_sub_text) + "\n");
}

std::string WebTestProxyBase::DumpBackForwardLists() {
  return DumpAllBackForwardLists(test_interfaces_, delegate_);
}

void LayoutAndPaintCallback::didLayoutAndPaint() {
  TRACE_EVENT0("shell", "LayoutAndPaintCallback::didLayoutAndPaint");
  if (wait_for_popup_) {
    wait_for_popup_ = false;
    return;
  }

  if (!callback_.is_null())
    callback_.Run();
  delete this;
}

void WebTestProxyBase::LayoutAndPaintAsyncThen(const base::Closure& callback) {
  TRACE_EVENT0("shell", "WebTestProxyBase::LayoutAndPaintAsyncThen");

  LayoutAndPaintCallback* layout_and_paint_callback =
      new LayoutAndPaintCallback(callback);
  web_widget_->layoutAndPaintAsync(layout_and_paint_callback);
  if (blink::WebPagePopup* popup = web_widget_->pagePopup()) {
    layout_and_paint_callback->set_wait_for_popup(true);
    popup->layoutAndPaintAsync(layout_and_paint_callback);
  }
}

void WebTestProxyBase::GetScreenOrientationForTesting(
    blink::WebScreenInfo& screen_info) {
  MockScreenOrientationClient* mock_client =
      test_interfaces_->GetTestRunner()->getMockScreenOrientationClient();
  if (mock_client->IsDisabled())
    return;
  // Override screen orientation information with mock data.
  screen_info.orientationType = mock_client->CurrentOrientationType();
  screen_info.orientationAngle = mock_client->CurrentOrientationAngle();
}

MockWebSpeechRecognizer* WebTestProxyBase::GetSpeechRecognizerMock() {
  if (!speech_recognizer_.get()) {
    speech_recognizer_.reset(new MockWebSpeechRecognizer());
    speech_recognizer_->SetDelegate(delegate_);
  }
  return speech_recognizer_.get();
}

MockCredentialManagerClient*
WebTestProxyBase::GetCredentialManagerClientMock() {
  if (!credential_manager_client_.get())
    credential_manager_client_.reset(new MockCredentialManagerClient());
  return credential_manager_client_.get();
}

void WebTestProxyBase::ScheduleAnimation() {
  if (!test_interfaces_->GetTestRunner()->TestIsRunning())
    return;

  if (!animate_scheduled_) {
    animate_scheduled_ = true;
    delegate_->PostDelayedTask(
        new HostMethodTask(this, &WebTestProxyBase::AnimateNow), 1);
  }
}

void WebTestProxyBase::AnimateNow() {
  if (animate_scheduled_) {
    base::TimeDelta animate_time = base::TimeTicks::Now() - base::TimeTicks();
    animate_scheduled_ = false;
    web_widget_->beginFrame(animate_time.InSecondsF());
    web_widget_->updateAllLifecyclePhases();
    if (blink::WebPagePopup* popup = web_widget_->pagePopup()) {
      popup->beginFrame(animate_time.InSecondsF());
      popup->updateAllLifecyclePhases();
    }
  }
}

void WebTestProxyBase::StartDragging(blink::WebLocalFrame* frame,
                                     const blink::WebDragData& data,
                                     blink::WebDragOperationsMask mask,
                                     const blink::WebImage& image,
                                     const blink::WebPoint& point) {
  test_interfaces_->GetTestRunner()->setDragImage(image);

  // When running a test, we need to fake a drag drop operation otherwise
  // Windows waits for real mouse events to know when the drag is over.
  test_interfaces_->GetEventSender()->DoDragDrop(data, mask);
}

// The output from these methods in layout test mode should match that
// expected by the layout tests. See EditingDelegate.m in DumpRenderTree.

void WebTestProxyBase::DidChangeContents() {
  if (test_interfaces_->GetTestRunner()->shouldDumpEditingCallbacks())
    delegate_->PrintMessage(
        "EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n");
}

bool WebTestProxyBase::CreateView(blink::WebLocalFrame* frame,
                                  const blink::WebURLRequest& request,
                                  const blink::WebWindowFeatures& features,
                                  const blink::WebString& frame_name,
                                  blink::WebNavigationPolicy policy,
                                  bool suppress_opener) {
  if (test_interfaces_->GetTestRunner()->shouldDumpNavigationPolicy()) {
    delegate_->PrintMessage("Default policy for createView for '" +
                            URLDescription(request.url()) + "' is '" +
                            WebNavigationPolicyToString(policy) + "'\n");
  }

  if (!test_interfaces_->GetTestRunner()->canOpenWindows())
    return false;
  if (test_interfaces_->GetTestRunner()->shouldDumpCreateView())
    delegate_->PrintMessage(std::string("createView(") +
                            URLDescription(request.url()) + ")\n");
  return true;
}

void WebTestProxyBase::SetStatusText(const blink::WebString& text) {
  if (!test_interfaces_->GetTestRunner()->shouldDumpStatusCallbacks())
    return;
  delegate_->PrintMessage(
      std::string("UI DELEGATE STATUS CALLBACK: setStatusText:") +
      text.utf8().data() + "\n");
}

// Simulate a print by going into print mode and then exit straight away.
void WebTestProxyBase::PrintPage(blink::WebLocalFrame* frame) {
  blink::WebSize page_size_in_pixels = web_widget_->size();
  if (page_size_in_pixels.isEmpty())
    return;
  blink::WebPrintParams printParams(page_size_in_pixels);
  frame->printBegin(printParams);
  frame->printEnd();
}

blink::WebSpeechRecognizer* WebTestProxyBase::GetSpeechRecognizer() {
  return GetSpeechRecognizerMock();
}

bool WebTestProxyBase::RequestPointerLock() {
  return test_interfaces_->GetTestRunner()->RequestPointerLock();
}

void WebTestProxyBase::RequestPointerUnlock() {
  test_interfaces_->GetTestRunner()->RequestPointerUnlock();
}

bool WebTestProxyBase::IsPointerLocked() {
  return test_interfaces_->GetTestRunner()->isPointerLocked();
}

void WebTestProxyBase::DidFocus() {
  delegate_->SetFocus(this, true);
}

void WebTestProxyBase::SetToolTipText(const blink::WebString& text,
                                      blink::WebTextDirection direction) {
  test_interfaces_->GetTestRunner()->setToolTipText(text);
}

void WebTestProxyBase::PostSpellCheckEvent(const blink::WebString& event_name) {
  if (test_interfaces_->GetTestRunner()->shouldDumpSpellCheckCallbacks()) {
    delegate_->PrintMessage(std::string("SpellCheckEvent: ") +
                            event_name.utf8().data() + "\n");
  }
}

void WebTestProxyBase::ResetInputMethod() {
  // If a composition text exists, then we need to let the browser process
  // to cancel the input method's ongoing composition session.
  if (web_widget_)
    web_widget_->confirmComposition();
}

blink::WebString WebTestProxyBase::acceptLanguages() {
  return blink::WebString::fromUTF8(
      test_interfaces_->GetTestRunner()->GetAcceptLanguages());
}

}  // namespace test_runner
