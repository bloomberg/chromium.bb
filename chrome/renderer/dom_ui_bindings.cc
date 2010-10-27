// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/dom_ui_bindings.h"

#include "base/json/json_writer.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"

DOMBoundBrowserObject::DOMBoundBrowserObject()
    : sender_(NULL),
      routing_id_(0) {
}

DOMBoundBrowserObject::~DOMBoundBrowserObject() {
  STLDeleteContainerPointers(properties_.begin(), properties_.end());
}

DOMUIBindings::DOMUIBindings() {
  BindMethod("send", &DOMUIBindings::send);
}

DOMUIBindings::~DOMUIBindings() {}

void DOMUIBindings::send(const CppArgumentList& args, CppVariant* result) {
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
    // TODO(evanm): we ought to support more than just sending arrays of
    // strings, but it's not yet necessary for the current code.
    std::vector<std::wstring> strings = args[1].ToStringVector();
    ListValue value;
    for (size_t i = 0; i < strings.size(); ++i) {
      // TODO(viettrungluu): remove conversion and utf_string_conversions.h
      value.Append(Value::CreateStringValue(WideToUTF16Hack(strings[i])));
    }
    base::JSONWriter::Write(&value, /* pretty_print= */ false, &content);
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
