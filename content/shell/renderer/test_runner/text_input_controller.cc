// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/text_input_controller.h"

#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

namespace content {

class TextInputControllerBindings
    : public gin::Wrappable<TextInputControllerBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(base::WeakPtr<TextInputController> controller,
                      blink::WebFrame* frame);

 private:
  explicit TextInputControllerBindings(
      base::WeakPtr<TextInputController> controller);
  virtual ~TextInputControllerBindings();

  // gin::Wrappable:
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

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

  base::WeakPtr<TextInputController> controller_;

  DISALLOW_COPY_AND_ASSIGN(TextInputControllerBindings);
};

gin::WrapperInfo TextInputControllerBindings::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void TextInputControllerBindings::Install(
    base::WeakPtr<TextInputController> controller,
    blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<TextInputControllerBindings> bindings =
      gin::CreateHandle(isolate, new TextInputControllerBindings(controller));
  if (bindings.IsEmpty())
    return;
  v8::Handle<v8::Object> global = context->Global();
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
      .SetMethod("setComposition",
                 &TextInputControllerBindings::SetComposition);
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

// TextInputController ---------------------------------------------------------

TextInputController::TextInputController()
    : view_(NULL), weak_factory_(this) {}

TextInputController::~TextInputController() {}

void TextInputController::Install(blink::WebFrame* frame) {
  TextInputControllerBindings::Install(weak_factory_.GetWeakPtr(), frame);
}

void TextInputController::SetWebView(blink::WebView* view) {
  view_ = view;
}

void TextInputController::InsertText(const std::string& text) {
  view_->confirmComposition(blink::WebString::fromUTF8(text));
}

void TextInputController::UnmarkText() {
  view_->confirmComposition();
}

void TextInputController::DoCommand(const std::string& text) {
  if (view_->mainFrame())
    view_->mainFrame()->executeCommand(blink::WebString::fromUTF8(text));
}

void TextInputController::SetMarkedText(const std::string& text,
                                        int start,
                                        int length) {
  blink::WebString web_text(blink::WebString::fromUTF8(text));

  // Split underline into up to 3 elements (before, selection, and after).
  std::vector<blink::WebCompositionUnderline> underlines;
  blink::WebCompositionUnderline underline;
  if (!start) {
    underline.endOffset = length;
  } else {
    underline.endOffset = start;
    underlines.push_back(underline);
    underline.startOffset = start;
    underline.endOffset = start + length;
  }
  underline.thick = true;
  underlines.push_back(underline);
  if (start + length < static_cast<int>(web_text.length())) {
    underline.startOffset = underline.endOffset;
    underline.endOffset = web_text.length();
    underline.thick = false;
    underlines.push_back(underline);
  }

  view_->setComposition(web_text, underlines, start, start + length);
}

bool TextInputController::HasMarkedText() {
  return view_->mainFrame() && view_->mainFrame()->hasMarkedText();
}

std::vector<int> TextInputController::MarkedRange() {
  if (!view_->mainFrame())
    return std::vector<int>();

  blink::WebRange range = view_->mainFrame()->markedRange();
  std::vector<int> int_array(2);
  int_array[0] = range.startOffset();
  int_array[1] = range.endOffset();

  return int_array;
}

std::vector<int> TextInputController::SelectedRange() {
  if (!view_->mainFrame())
    return std::vector<int>();

  blink::WebRange range = view_->mainFrame()->selectionRange();
  std::vector<int> int_array(2);
  int_array[0] = range.startOffset();
  int_array[1] = range.endOffset();

  return int_array;
}

std::vector<int> TextInputController::FirstRectForCharacterRange(
    unsigned location,
    unsigned length) {
  blink::WebRect rect;
  if (!view_->focusedFrame() ||
      !view_->focusedFrame()->firstRectForCharacterRange(
          location, length, rect)) {
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
  blink::WebKeyboardEvent key_down;
  key_down.type = blink::WebInputEvent::RawKeyDown;
  key_down.modifiers = 0;
  key_down.windowsKeyCode = 0xE5;  // VKEY_PROCESSKEY
  key_down.setKeyIdentifierFromWindowsKeyCode();
  view_->handleInputEvent(key_down);

  blink::WebVector<blink::WebCompositionUnderline> underlines;
  blink::WebString web_text(blink::WebString::fromUTF8(text));
  view_->setComposition(web_text, underlines, 0, web_text.length());
}

}  // namespace content
