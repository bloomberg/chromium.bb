// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/web_view_test_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/test_runner/event_sender.h"
#include "components/test_runner/mock_web_speech_recognizer.h"
#include "components/test_runner/test_common.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/web_task.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_test_proxy.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWidget.h"

namespace test_runner {

WebViewTestClient::WebViewTestClient(TestRunner* test_runner,
                                     WebTestProxyBase* web_test_proxy_base)
    : test_runner_(test_runner),
      web_test_proxy_base_(web_test_proxy_base),
      animation_scheduled_(false),
      weak_factory_(this) {
  DCHECK(test_runner);
  DCHECK(web_test_proxy_base);
}

WebViewTestClient::~WebViewTestClient() {}

void WebViewTestClient::scheduleAnimation() {
  if (!test_runner_->TestIsRunning())
    return;

  if (!animation_scheduled_) {
    animation_scheduled_ = true;
    test_runner_->OnAnimationScheduled(web_test_proxy_base_->web_view());

    delegate()->PostDelayedTask(
        new WebCallbackTask(base::Bind(&WebViewTestClient::AnimateNow,
                                       weak_factory_.GetWeakPtr())),
        1);
  }
}

void WebViewTestClient::AnimateNow() {
  if (animation_scheduled_) {
    blink::WebWidget* web_widget = web_test_proxy_base_->web_widget();
    animation_scheduled_ = false;
    test_runner_->OnAnimationBegun(web_test_proxy_base_->web_view());

    base::TimeDelta animate_time = base::TimeTicks::Now() - base::TimeTicks();
    web_widget->beginFrame(animate_time.InSecondsF());
    web_widget->updateAllLifecyclePhases();
    if (blink::WebPagePopup* popup = web_widget->pagePopup()) {
      popup->beginFrame(animate_time.InSecondsF());
      popup->updateAllLifecyclePhases();
    }
  }
}

void WebViewTestClient::startDragging(blink::WebLocalFrame* frame,
                                      const blink::WebDragData& data,
                                      blink::WebDragOperationsMask mask,
                                      const blink::WebImage& image,
                                      const blink::WebPoint& point) {
  test_runner_->setDragImage(image);

  // When running a test, we need to fake a drag drop operation otherwise
  // Windows waits for real mouse events to know when the drag is over.
  web_test_proxy_base_->event_sender()->DoDragDrop(data, mask);
}

// The output from these methods in layout test mode should match that
// expected by the layout tests. See EditingDelegate.m in DumpRenderTree.

void WebViewTestClient::didChangeContents() {
  if (test_runner_->shouldDumpEditingCallbacks())
    delegate()->PrintMessage(
        "EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n");
}

blink::WebView* WebViewTestClient::createView(
    blink::WebLocalFrame* frame,
    const blink::WebURLRequest& request,
    const blink::WebWindowFeatures& features,
    const blink::WebString& frame_name,
    blink::WebNavigationPolicy policy,
    bool suppress_opener) {
  if (test_runner_->shouldDumpNavigationPolicy()) {
    delegate()->PrintMessage("Default policy for createView for '" +
                             URLDescription(request.url()) + "' is '" +
                             WebNavigationPolicyToString(policy) + "'\n");
  }

  if (!test_runner_->canOpenWindows())
    return nullptr;
  if (test_runner_->shouldDumpCreateView())
    delegate()->PrintMessage(std::string("createView(") +
                             URLDescription(request.url()) + ")\n");

  // The return value below is used to communicate to WebTestProxy whether it
  // should forward the createView request to RenderViewImpl or not.  The
  // somewhat ugly cast is used to do this while fitting into the existing
  // WebViewClient interface.
  return reinterpret_cast<blink::WebView*>(0xdeadbeef);
}

void WebViewTestClient::setStatusText(const blink::WebString& text) {
  if (!test_runner_->shouldDumpStatusCallbacks())
    return;
  delegate()->PrintMessage(
      std::string("UI DELEGATE STATUS CALLBACK: setStatusText:") +
      text.utf8().data() + "\n");
}

// Simulate a print by going into print mode and then exit straight away.
void WebViewTestClient::printPage(blink::WebLocalFrame* frame) {
  blink::WebSize page_size_in_pixels = frame->view()->size();
  if (page_size_in_pixels.isEmpty())
    return;
  blink::WebPrintParams printParams(page_size_in_pixels);
  frame->printBegin(printParams);
  frame->printEnd();
}

bool WebViewTestClient::runFileChooser(
    const blink::WebFileChooserParams& params,
    blink::WebFileChooserCompletion* completion) {
  delegate()->PrintMessage("Mock: Opening a file chooser.\n");
  // FIXME: Add ability to set file names to a file upload control.
  return false;
}

void WebViewTestClient::showValidationMessage(
    const blink::WebRect& anchor_in_root_view,
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
  delegate()->PrintMessage("ValidationMessageClient: main-message=" +
                           base::UTF16ToUTF8(wrapped_main_text) +
                           " sub-message=" +
                           base::UTF16ToUTF8(wrapped_sub_text) + "\n");
}

blink::WebSpeechRecognizer* WebViewTestClient::speechRecognizer() {
  return test_runner_->getMockWebSpeechRecognizer();
}

bool WebViewTestClient::requestPointerLock() {
  return test_runner_->RequestPointerLock();
}

void WebViewTestClient::requestPointerUnlock() {
  test_runner_->RequestPointerUnlock();
}

bool WebViewTestClient::isPointerLocked() {
  return test_runner_->isPointerLocked();
}

void WebViewTestClient::didFocus() {
  test_runner_->SetFocus(web_test_proxy_base_->web_view(), true);
}

void WebViewTestClient::setToolTipText(const blink::WebString& text,
                                       blink::WebTextDirection direction) {
  test_runner_->setToolTipText(text);
}

void WebViewTestClient::resetInputMethod() {
  // If a composition text exists, then we need to let the browser process
  // to cancel the input method's ongoing composition session.
  if (web_test_proxy_base_)
    web_test_proxy_base_->web_widget()->confirmComposition();
}

blink::WebString WebViewTestClient::acceptLanguages() {
  return blink::WebString::fromUTF8(test_runner_->GetAcceptLanguages());
}

WebTestDelegate* WebViewTestClient::delegate() {
  return web_test_proxy_base_->delegate();
}

}  // namespace test_runner
