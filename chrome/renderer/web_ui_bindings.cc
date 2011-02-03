// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/web_ui_bindings.h"

#include "base/json/json_writer.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"

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

DOMBoundBrowserObject::DOMBoundBrowserObject()
    : sender_(NULL),
      routing_id_(0) {
}

DOMBoundBrowserObject::~DOMBoundBrowserObject() {
  STLDeleteContainerPointers(properties_.begin(), properties_.end());
}

WebUIBindings::WebUIBindings() {
  BindMethod("send", &WebUIBindings::send);
}

WebUIBindings::~WebUIBindings() {}

void WebUIBindings::send(const CppArgumentList& args, CppVariant* result) {
  // We expect at least a string message identifier, and optionally take
  // an object parameter.  If we get anything else we bail.
  if (args.size() < 1 || args.size() > 2)
    return;

  // Require the first parameter to be the message name.
  if (!args[0].isString())
    return;
  const std::string message = args[0].ToString();

  // If they've provided an optional message parameter, convert that into JSON.
  std::string content;
  if (args.size() == 2) {
    if (!args[1].isObject())
      return;

    scoped_ptr<Value> value(CreateValueFromCppVariant(args[1]));
    base::JSONWriter::Write(value.get(), /* pretty_print= */ false, &content);
  }

  // Retrieve the source frame's url
  GURL source_url;
  WebKit::WebFrame* webframe = WebKit::WebFrame::frameForCurrentContext();
  if (webframe)
    source_url = webframe->url();

  // Send the message up to the browser.
  sender()->Send(
      new ViewHostMsg_DOMUISend(routing_id(), source_url, message, content));
}

void DOMBoundBrowserObject::SetProperty(const std::string& name,
                                const std::string& value) {
  CppVariant* cpp_value = new CppVariant;
  cpp_value->Set(value);
  BindProperty(name, cpp_value);
  properties_.push_back(cpp_value);
}
