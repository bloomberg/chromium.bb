// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/virtual_keyboard/public/virtual_keyboard_bindings.h"

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_observer.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "ui/keyboard/keyboard_constants.h"

namespace {

struct VKEvent {
  std::string event_type;
  int char_value;
  int key_code;
  std::string key_name;
  int modifiers;
};

}  // namespace

namespace gin {

template <>
struct Converter<VKEvent> {
  static bool FromV8(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     VKEvent* event) {
    if (!val->IsObject())
      return false;
    v8::Handle<v8::Object> dict(v8::Handle<v8::Object>::Cast(val));

    if (!Converter<std::string>::FromV8(
            isolate,
            dict->Get(v8::String::NewFromUtf8(isolate, "type")),
            &event->event_type))
      return false;
    if (!Converter<int32_t>::FromV8(
            isolate,
            dict->Get(v8::String::NewFromUtf8(isolate, "charValue")),
            &event->char_value))
      return false;
    if (!Converter<int32_t>::FromV8(
            isolate,
            dict->Get(v8::String::NewFromUtf8(isolate, "keyCode")),
            &event->key_code))
      return false;
    if (!Converter<std::string>::FromV8(
            isolate,
            dict->Get(v8::String::NewFromUtf8(isolate, "keyName")),
            &event->key_name))
      return false;
    if (!Converter<int32_t>::FromV8(
            isolate,
            dict->Get(v8::String::NewFromUtf8(isolate, "modifiers")),
            &event->modifiers))
      return false;

    return true;
  }
};

}  // namespace gin

namespace athena {

namespace {

class VKBindings : public gin::Wrappable<VKBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(content::RenderView* render_view) {
    blink::WebFrame* web_frame =
        render_view->GetMainRenderFrame()->GetWebFrame();
    v8::Isolate* isolate = blink::mainThreadIsolate();
    v8::HandleScope handle_scope(isolate);
    v8::Handle<v8::Context> context = web_frame->mainWorldScriptContext();
    if (context.IsEmpty())
      return;

    v8::Context::Scope context_scope(context);

    v8::Handle<v8::Object> global = context->Global();
    v8::Handle<v8::Object> chrome =
        global->Get(gin::StringToV8(isolate, "chrome"))->ToObject();
    CHECK(!chrome.IsEmpty());

    gin::Handle<VKBindings> controller =
        gin::CreateHandle(isolate, new VKBindings(render_view));
    if (controller.IsEmpty())
      return;
    chrome->Set(gin::StringToSymbol(isolate, "virtualKeyboardPrivate"),
                controller.ToV8());

    const std::string kInputBoxFocusedEvent =
        "chrome.virtualKeyboardPrivate.onTextInputBoxFocused = {};"
        "chrome.virtualKeyboardPrivate.onTextInputBoxFocused.addListener = "
        "   function(callback) { "
        "     window.setTimeout(function() {"
        "       callback({type: 'text'});"
        "     }, 100);"
        "   };";
    render_view->GetMainRenderFrame()->ExecuteJavaScript(
        base::UTF8ToUTF16(kInputBoxFocusedEvent));
  }

 private:
  explicit VKBindings(content::RenderView* render_view)
      : render_view_(render_view) {}
  virtual ~VKBindings() {}

  // gin::WrappableBase
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE {
    return gin::Wrappable<VKBindings>::GetObjectTemplateBuilder(isolate)
        .SetMethod("moveCursor", &VKBindings::NotImplemented)
        .SetMethod("sendKeyEvent", &VKBindings::SendKeyEvent)
        .SetMethod("hideKeyboard", &VKBindings::HideKeyboard)
        .SetMethod("lockKeyboard", &VKBindings::NotImplemented)
        .SetMethod("keyboardLoaded", &VKBindings::NotImplemented)
        .SetMethod("getKeyboardConfig", &VKBindings::NotImplemented);
  }

  void SendKeyEvent(gin::Arguments* args) {
    VKEvent event;
    if (!args->GetNext(&event)) {
      LOG(ERROR) << "Failed to get the type";
      return;
    }
    base::ListValue params;
    params.Set(0, new base::StringValue(event.event_type));
    params.Set(1, new base::FundamentalValue(event.char_value));
    params.Set(2, new base::FundamentalValue(event.key_code));
    params.Set(3, new base::StringValue(event.key_name));
    params.Set(4, new base::FundamentalValue(event.modifiers));

    std::string params_json;
    JSONStringValueSerializer serializer(&params_json);
    if (!serializer.Serialize(params))
      return;

    render_view_->GetMainRenderFrame()->ExecuteJavaScript(
        base::UTF8ToUTF16("chrome.send('sendKeyEvent', " + params_json + ")"));
  }

  void HideKeyboard() {
    render_view_->GetMainRenderFrame()->ExecuteJavaScript(
        base::UTF8ToUTF16("chrome.send('hideKeyboard', [])"));
  }

  void NotImplemented(gin::Arguments* args) { NOTIMPLEMENTED(); }

  content::RenderView* render_view_;

  DISALLOW_COPY_AND_ASSIGN(VKBindings);
};

gin::WrapperInfo VKBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

class VirtualKeyboardBindingsImpl : public VirtualKeyboardBindings,
                                    public content::RenderViewObserver {
 public:
  explicit VirtualKeyboardBindingsImpl(content::RenderView* render_view)
      : content::RenderViewObserver(render_view) {}

  virtual ~VirtualKeyboardBindingsImpl() {}

 private:
  // content::RenderViewObserver:
  virtual void Navigate(const GURL& url) OVERRIDE {
    bool enabled_bindings = render_view()->GetEnabledBindings();
    if (!(enabled_bindings & content::BINDINGS_POLICY_WEB_UI))
      return;
    if (url.GetOrigin() == GURL(keyboard::kKeyboardURL))
      VKBindings::Install(render_view());
  }
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardBindingsImpl);
};

}  // namespace

VirtualKeyboardBindings* VirtualKeyboardBindings::Create(
    content::RenderView* render_view) {
  return new VirtualKeyboardBindingsImpl(render_view);
}

}  // namespace athena
