// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js/js_event_details.h"

#include "base/json/json_writer.h"

namespace browser_sync {

JsEventDetails::JsEventDetails()
    : details_(new SharedDictionaryValue()) {}

JsEventDetails::JsEventDetails(DictionaryValue* details)
    : details_(new SharedDictionaryValue(details)) {}

JsEventDetails::~JsEventDetails() {}

const DictionaryValue& JsEventDetails::Get() const {
  return details_->Get();
}

std::string JsEventDetails::ToString() const {
  std::string str;
  base::JSONWriter::Write(&Get(), false, &str);
  return str;
}

}  // namespace browser_sync
