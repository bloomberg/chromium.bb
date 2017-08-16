// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/text_input_controller.h"

#include "base/macros.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/platform/WebCoalescedInputEvent.h"
#include "third_party/WebKit/public/platform/WebInputEventResult.h"
#include "third_party/WebKit/public/platform/WebKeyboardEvent.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebImeTextSpan.h"
#include "third_party/WebKit/public/web/WebInputMethodController.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/base_event_utils.h"
#include "v8/include/v8.h"

namespace test_runner {

class TextInputControllerBindings
    : public gin::Wrappable<TextInputControllerBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(base::WeakPtr<TextInputController> controller,
                      blink::WebLocalFrame* frame);

 private:
  explicit TextInputControllerBindings(
      base::WeakPtr<TextInputController> controller);
  ~TextInputControllerBindings() override;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  void InsertText(const std::string& text);
  void UnmarkText();
  void DoCommand(const std::string& text);
  void SetMarkedText(const std::string& text, int start, int length);
  bool HasMarkedText();
  std::vector<int> MarkedRange();
  std::vector<int> SelectedRange();
  std::vector<int> FirstRectForCharacterRange(unsigned location,
                                              unsigned length);
  void SetComposition(const std::string& text);
  void ForceTextInputStateUpdate();

  base::WeakPtr<TextInputController> controller_;

  DISALLOW_COPY_AND_ASSIGN(TextInputControllerBindings);
};

gin::WrapperInfo TextInputControllerBindings::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void TextInputControllerBindings::Install(
    base::WeakPtr<TextInputController> controller,
    blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<TextInputControllerBindings> bindings =
      gin::CreateHandle(isolate, new TextInputControllerBindings(controller));
  if (bindings.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "textInputController"), bindings.ToV8());
}

TextInputControllerBindings::TextInputControllerBindings(
    base::WeakPtr<TextInputController> controller)
    : controller_(controller) {}

TextInputControllerBindings::~TextInputControllerBindings() {}

gin::ObjectTemplateBuilder
TextInputControllerBindings::GetObjectTemplateBuilder(v8::Isolate* isolate) {
  return gin::Wrappable<TextInputControllerBindings>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("insertText", &TextInputControllerBindings::InsertText)
      .SetMethod("unmarkText", &TextInputControllerBindings::UnmarkText)
      .SetMethod("doCommand", &TextInputControllerBindings::DoCommand)
      .SetMethod("setMarkedText", &TextInputControllerBindings::SetMarkedText)
      .SetMethod("hasMarkedText", &TextInputControllerBindings::HasMarkedText)
      .SetMethod("markedRange", &TextInputControllerBindings::MarkedRange)
      .SetMethod("selectedRange", &TextInputControllerBindings::SelectedRange)
      .SetMethod("firstRectForCharacterRange",
                 &TextInputControllerBindings::FirstRectForCharacterRange)
      .SetMethod("setComposition", &TextInputControllerBindings::SetComposition)
      .SetMethod("forceTextInputStateUpdate",
                 &TextInputControllerBindings::ForceTextInputStateUpdate);
}

void TextInputControllerBindings::InsertText(const std::string& text) {
  if (controller_)
    controller_->InsertText(text);
}

void TextInputControllerBindings::UnmarkText() {
  if (controller_)
    controller_->UnmarkText();
}

void TextInputControllerBindings::DoCommand(const std::string& text) {
  if (controller_)
    controller_->DoCommand(text);
}

void TextInputControllerBindings::SetMarkedText(const std::string& text,
                                                int start,
                                                int length) {
  if (controller_)
    controller_->SetMarkedText(text, start, length);
}

bool TextInputControllerBindings::HasMarkedText() {
  return controller_ ? controller_->HasMarkedText() : false;
}

std::vector<int> TextInputControllerBindings::MarkedRange() {
  return controller_ ? controller_->MarkedRange() : std::vector<int>();
}

std::vector<int> TextInputControllerBindings::SelectedRange() {
  return controller_ ? controller_->SelectedRange() : std::vector<int>();
}

std::vector<int> TextInputControllerBindings::FirstRectForCharacterRange(
    unsigned location,
    unsigned length) {
  return controller_ ? controller_->FirstRectForCharacterRange(location, length)
                     : std::vector<int>();
}

void TextInputControllerBindings::SetComposition(const std::string& text) {
  if (controller_)
    controller_->SetComposition(text);
}
void TextInputControllerBindings::ForceTextInputStateUpdate() {
  if (controller_)
    controller_->ForceTextInputStateUpdate();
}
// TextInputController ---------------------------------------------------------

TextInputController::TextInputController(
    WebViewTestProxyBase* web_view_test_proxy_base)
    : web_view_test_proxy_base_(web_view_test_proxy_base),
      weak_factory_(this) {}

TextInputController::~TextInputController() {}

void TextInputController::Install(blink::WebLocalFrame* frame) {
  TextInputControllerBindings::Install(weak_factory_.GetWeakPtr(), frame);
}

void TextInputController::InsertText(const std::string& text) {
  if (auto* controller = GetInputMethodController()) {
    controller->CommitText(blink::WebString::FromUTF8(text),
                           std::vector<blink::WebImeTextSpan>(),
                           blink::WebRange(), 0);
  }
}

void TextInputController::UnmarkText() {
  if (auto* controller = GetInputMethodController()) {
    controller->FinishComposingText(
        blink::WebInputMethodController::kKeepSelection);
  }
}

void TextInputController::DoCommand(const std::string& text) {
  if (view()->MainFrame()) {
    if (!view()->MainFrame()->ToWebLocalFrame()) {
      CHECK(false) << "This function cannot be called if the main frame is not"
                      "a local frame.";
    }
    view()->MainFrame()->ToWebLocalFrame()->ExecuteCommand(
        blink::WebString::FromUTF8(text));
  }
}

void TextInputController::SetMarkedText(const std::string& text,
                                        int start,
                                        int length) {
  blink::WebString web_text(blink::WebString::FromUTF8(text));

  // Split underline into up to 3 elements (before, selection, and after).
  std::vector<blink::WebImeTextSpan> ime_text_spans;
  blink::WebImeTextSpan ime_text_span;
  if (!start) {
    ime_text_span.end_offset = length;
  } else {
    ime_text_span.end_offset = start;
    ime_text_spans.push_back(ime_text_span);
    ime_text_span.start_offset = start;
    ime_text_span.end_offset = start + length;
  }
  ime_text_span.thick = true;
  ime_text_spans.push_back(ime_text_span);
  if (start + length < static_cast<int>(web_text.length())) {
    ime_text_span.start_offset = ime_text_span.end_offset;
    ime_text_span.end_offset = web_text.length();
    ime_text_span.thick = false;
    ime_text_spans.push_back(ime_text_span);
  }

  if (auto* controller = GetInputMethodController()) {
    controller->SetComposition(web_text, ime_text_spans, blink::WebRange(),
                               start, start + length);
  }
}

bool TextInputController::HasMarkedText() {
  if (!view()->MainFrame())
    return false;

  if (!view()->MainFrame()->ToWebLocalFrame()) {
    CHECK(false) << "This function cannot be called if the main frame is not"
                    "a local frame.";
  }

  return view()->MainFrame()->ToWebLocalFrame()->HasMarkedText();
}

std::vector<int> TextInputController::MarkedRange() {
  if (!view()->MainFrame())
    return std::vector<int>();

  if (!view()->MainFrame()->ToWebLocalFrame()) {
    CHECK(false) << "This function cannot be called if the main frame is not"
                    "a local frame.";
  }

  blink::WebRange range = view()->MainFrame()->ToWebLocalFrame()->MarkedRange();
  std::vector<int> int_array(2);
  int_array[0] = range.StartOffset();
  int_array[1] = range.EndOffset();

  return int_array;
}

std::vector<int> TextInputController::SelectedRange() {
  if (!view()->MainFrame())
    return std::vector<int>();

  if (!view()->MainFrame()->ToWebLocalFrame()) {
    CHECK(false) << "This function cannot be called if the main frame is not"
                    "a local frame.";
  }

  blink::WebRange range =
      view()->MainFrame()->ToWebLocalFrame()->SelectionRange();
  if (range.IsNull())
    return std::vector<int>();
  std::vector<int> int_array(2);
  int_array[0] = range.StartOffset();
  int_array[1] = range.EndOffset();

  return int_array;
}

std::vector<int> TextInputController::FirstRectForCharacterRange(
    unsigned location,
    unsigned length) {
  blink::WebRect rect;
  if (!view()->FocusedFrame() ||
      !view()->FocusedFrame()->FirstRectForCharacterRange(location, length,
                                                          rect)) {
    return std::vector<int>();
  }

  std::vector<int> int_array(4);
  int_array[0] = rect.x;
  int_array[1] = rect.y;
  int_array[2] = rect.width;
  int_array[3] = rect.height;

  return int_array;
}

void TextInputController::SetComposition(const std::string& text) {
  // Sends a keydown event with key code = 0xE5 to emulate input method
  // behavior.
  blink::WebKeyboardEvent key_down(
      blink::WebInputEvent::kRawKeyDown, blink::WebInputEvent::kNoModifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));

  key_down.windows_key_code = 0xE5;  // VKEY_PROCESSKEY
  view()->HandleInputEvent(blink::WebCoalescedInputEvent(key_down));

  // The value returned by std::string::length() may not correspond to the
  // actual number of encoded characters in sequences of multi-byte or
  // variable-length characters.
  blink::WebString newText = blink::WebString::FromUTF8(text);
  size_t textLength = newText.length();

  std::vector<blink::WebImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(blink::WebImeTextSpan(
      blink::WebImeTextSpan::Type::kComposition, 0, textLength, SK_ColorBLACK,
      false, SK_ColorTRANSPARENT));
  if (auto* controller = GetInputMethodController()) {
    controller->SetComposition(
        newText, blink::WebVector<blink::WebImeTextSpan>(ime_text_spans),
        blink::WebRange(), textLength, textLength);
  }
}

void TextInputController::ForceTextInputStateUpdate() {
  // TODO(lukasza): Finish adding OOPIF support to the layout tests harness.
  CHECK(view()->MainFrame()->IsWebLocalFrame())
      << "WebView does not have a local main frame and"
         " cannot handle input method controller tasks.";
  web_view_test_proxy_base_->delegate()->ForceTextInputStateUpdate(
      view()->MainFrame()->ToWebLocalFrame());
}

blink::WebView* TextInputController::view() {
  return web_view_test_proxy_base_->web_view();
}

blink::WebInputMethodController*
TextInputController::GetInputMethodController() {
  if (!view()->MainFrame())
    return nullptr;

  // TODO(lukasza): Finish adding OOPIF support to the layout tests harness.
  CHECK(view()->MainFrame()->IsWebLocalFrame())
      << "WebView does not have a local main frame and"
         " cannot handle input method controller tasks.";

  return view()
      ->MainFrame()
      ->ToWebLocalFrame()
      ->FrameWidget()
      ->GetActiveWebInputMethodController();
}

}  // namespace test_runner
