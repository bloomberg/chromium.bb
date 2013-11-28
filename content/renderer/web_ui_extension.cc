// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_ui_extension.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/common/view_messages.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/renderer/web_ui_extension_data.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace content {

static const char* const kWebUIExtensionName = "v8/WebUI";

// Javascript that gets executed when the extension loads into all frames.
// Exposes two methods:
//  - chrome.send: Used to send messages to the browser. Requires the message
//      name as the first argument and can have an optional second argument that
//      should be an array.
//  - chrome.getVariableValue: Returns value for the input variable name if such
//      a value was set by the browser. Else will return an empty string.
static const char* const kWebUIExtensionJS =
    "var chrome;"
    "if (!chrome)"
    "  chrome = {};"
    "chrome.send = function(name, data) {"
    "  native function Send();"
    "  Send(name, data);"
    "};"
    "chrome.getVariableValue = function(name) {"
    "  native function GetVariableValue();"
    "  return GetVariableValue(name);"
    "};";

class WebUIExtensionWrapper : public v8::Extension {
 public:
  WebUIExtensionWrapper();
  virtual ~WebUIExtensionWrapper();

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Handle<v8::String> name) OVERRIDE;
  static void Send(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetVariableValue(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  static bool ShouldRespondToRequest(blink::WebFrame** frame_ptr,
                                     RenderView** render_view_ptr);

  DISALLOW_COPY_AND_ASSIGN(WebUIExtensionWrapper);
};

WebUIExtensionWrapper::WebUIExtensionWrapper()
    : v8::Extension(kWebUIExtensionName, kWebUIExtensionJS) {}

WebUIExtensionWrapper::~WebUIExtensionWrapper() {}

v8::Handle<v8::FunctionTemplate>
WebUIExtensionWrapper::GetNativeFunctionTemplate(v8::Isolate* isolate,
                                                 v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::NewFromUtf8(isolate, "Send")))
    return v8::FunctionTemplate::New(isolate, Send);
  if (name->Equals(v8::String::NewFromUtf8(isolate, "GetVariableValue")))
    return v8::FunctionTemplate::New(isolate, GetVariableValue);
  return v8::Handle<v8::FunctionTemplate>();
}

// static
bool WebUIExtensionWrapper::ShouldRespondToRequest(
    blink::WebFrame** frame_ptr,
    RenderView** render_view_ptr) {
  blink::WebFrame* frame = blink::WebFrame::frameForCurrentContext();
  if (!frame || !frame->view())
    return false;

  RenderView* render_view = RenderView::FromWebView(frame->view());
  if (!render_view)
    return false;

  GURL frame_url = frame->document().url();

  bool webui_enabled =
      (render_view->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI) &&
      (frame_url.SchemeIs(chrome::kChromeUIScheme) ||
       frame_url.SchemeIs(chrome::kDataScheme));

  if (!webui_enabled)
    return false;

  *frame_ptr = frame;
  *render_view_ptr = render_view;
  return true;
}

// static
void WebUIExtensionWrapper::Send(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  blink::WebFrame* frame;
  RenderView* render_view;
  if (!ShouldRespondToRequest(&frame, &render_view))
    return;

  // We expect at least two parameters - a string message identifier, and
  // an object parameter. The object param can be undefined.
  if (args.Length() != 2 || !args[0]->IsString())
    return;

  const std::string message = *v8::String::Utf8Value(args[0]->ToString());

  // If they've provided an optional message parameter, convert that into a
  // Value to send to the browser process.
  scoped_ptr<ListValue> content;
  if (args[1]->IsUndefined()) {
    content.reset(new ListValue());
  } else {
    if (!args[1]->IsObject())
      return;

    scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());

    base::Value* value = converter->FromV8Value(
        args[1], frame->mainWorldScriptContext());
    base::ListValue* list = NULL;
    value->GetAsList(&list);
    DCHECK(list);
    content.reset(list);
  }

  // Send the message up to the browser.
  render_view->Send(new ViewHostMsg_WebUISend(render_view->GetRoutingID(),
                                              frame->document().url(),
                                              message,
                                              *content));
}

// static
void WebUIExtensionWrapper::GetVariableValue(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  blink::WebFrame* frame;
  RenderView* render_view;
  if (!ShouldRespondToRequest(&frame, &render_view))
    return;

  if (!args.Length() || !args[0]->IsString())
    return;

  std::string key = *v8::String::Utf8Value(args[0]->ToString());
  std::string value = WebUIExtensionData::Get(render_view)->GetValue(key);
  args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(),
                                                    value.c_str(),
                                                    v8::String::kNormalString,
                                                    value.length()));
}

// static
v8::Extension* WebUIExtension::Get() {
  return new WebUIExtensionWrapper();
}

}  // namespace content
