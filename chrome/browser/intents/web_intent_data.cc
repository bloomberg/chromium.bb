// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intent_data.h"

WebIntentData::WebIntentData() {}

WebIntentData::~WebIntentData() {}

bool WebIntentData::operator==(const WebIntentData& other) const {
  return (service_url == other.service_url &&
          action == other.action &&
          type == other.type);
}
