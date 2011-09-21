// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/web_intent_service_data.h"
#include <ostream>

WebIntentServiceData::WebIntentServiceData()
    : disposition(WebIntentServiceData::DISPOSITION_WINDOW) {
}

WebIntentServiceData::~WebIntentServiceData() {}

bool WebIntentServiceData::operator==(const WebIntentServiceData& other) const {
  return (service_url == other.service_url &&
          action == other.action &&
          type == other.type &&
          title == other.title &&
          disposition == other.disposition);
}

std::ostream& operator<<(::std::ostream& os,
                         const WebIntentServiceData& intent) {
  return os <<
         "{" << intent.service_url <<
         ", " << UTF16ToUTF8(intent.action) <<
         ", " << UTF16ToUTF8(intent.type) <<
         ", " << UTF16ToUTF8(intent.title) <<
         ", " << intent.disposition <<
         "}";
}
