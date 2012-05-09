// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_ui_bindings.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "content/common/view_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"

using webkit_glue::CppArgumentList;
using webkit_glue::CppVariant;

namespace {

// Creates a Value which is a copy of the CppVariant |value|. All objects are
// treated as Lists for now since CppVariant does not distinguish arrays in any
// convenient way and we currently have no need of non array objects.
Value* CreateValueFromCppVariant(const CppVariant& value) {
  if (value.isBool())
    return Value::CreateBooleanValue(value.ToBoolean());
  if (value.isDouble())
    return Value::CreateDoubleValue(value.ToDouble());
  if (value.isInt32())
    return Value::CreateIntegerValue(value.ToInt32());
  if (value.isString())
    return Value::CreateStringValue(value.ToString());

  if (value.isObject()) {
    // We currently assume all objects are arrays.
    std::vector<CppVariant> vector = value.ToVector();
    ListValue* list = new ListValue();
    for (size_t i = 0; i < vector.size(); ++i) {
      list->Append(CreateValueFromCppVariant(vector[i]));
    }
    return list;
  }

  // Covers null and undefined.
  return Value::CreateNullValue();
}

}  // namespace

DOMBoundBrowserObject::DOMBoundBrowserObject() {
}

DOMBoundBrowserObject::~DOMBoundBrowserObject() {
  STLDeleteContainerPointers(properties_.begin(), properties_.end());
}

WebUIBindings::WebUIBindings(IPC::Message::Sender* sender, int routing_id)
    : sender_(sender), routing_id_(routing_id) {
  BindCallback("send", base::Bind(&WebUIBindings::Send,
                                  base::Unretained(this)));
}

WebUIBindings::~WebUIBindings() {}

void WebUIBindings::Send(const CppArgumentList& args, CppVariant* result) {
  // We expect at least a string message identifier, and optionally take
  // an object parameter.  If we get anything else we bail.
  if (args.size() < 1 || args.size() > 2)
    return;

  // Require the first parameter to be the message name.
  if (!args[0].isString())
    return;
  const std::string message = args[0].ToString();

  // If they've provided an optional message parameter, convert that into a
  // Value to send to the browser process.
  scoped_ptr<Value> content;
  if (args.size() == 2) {
    if (!args[1].isObject())
      return;

    content.reset(CreateValueFromCppVariant(args[1]));
    CHECK(content->IsType(Value::TYPE_LIST));
  } else {
    content.reset(new ListValue());
  }

  // Retrieve the source document's url
  GURL source_url;
  WebKit::WebFrame* frame = WebKit::WebFrame::frameForCurrentContext();
  if (frame)
    source_url = frame->document().url();

  // Send the message up to the browser.
  sender_->Send(new ViewHostMsg_WebUISend(
      routing_id_,
      source_url,
      message,
      *(static_cast<ListValue*>(content.get()))));
}

void DOMBoundBrowserObject::SetProperty(const std::string& name,
                                        const std::string& value) {
  CppVariant* cpp_value = new CppVariant;
  cpp_value->Set(value);
  BindProperty(name, cpp_value);
  properties_.push_back(cpp_value);
}
