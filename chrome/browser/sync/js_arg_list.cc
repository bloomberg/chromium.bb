// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_arg_list.h"

#include "base/json/json_writer.h"
#include "base/scoped_ptr.h"

namespace browser_sync {

JsArgList::JsArgList() : args_(new SharedListValue()) {}

JsArgList::JsArgList(const ListValue& args)
    : args_(new SharedListValue(args)) {}

JsArgList::JsArgList(const std::vector<const Value*>& args)
    : args_(new SharedListValue(args)) {}

const ListValue& JsArgList::Get() const {
  return args_->Get();
}

std::string JsArgList::ToString() const {
  std::string str;
  base::JSONWriter::Write(&Get(), false, &str);
  return str;
}

JsArgList::SharedListValue::SharedListValue() {}

JsArgList::SharedListValue::SharedListValue(const ListValue& list_value) {
  // Go through contortions to copy the list since ListValues are not
  // copyable.
  scoped_ptr<ListValue> list_value_copy(list_value.DeepCopy());
  list_value_.Swap(list_value_copy.get());
}

JsArgList::SharedListValue::SharedListValue(
    const std::vector<const Value*>& value_list) {
  for (std::vector<const Value*>::const_iterator it = value_list.begin();
       it != value_list.end(); ++it) {
    list_value_.Append((*it)->DeepCopy());
  }
}

const ListValue& JsArgList::SharedListValue::Get() const {
  return list_value_;
}

JsArgList::SharedListValue::~SharedListValue() {}

}  // namespace browser_sync
