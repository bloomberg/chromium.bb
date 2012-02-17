// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/default_web_intent_service.h"

DefaultWebIntentService::DefaultWebIntentService()
  : url_pattern(URLPattern::SCHEME_ALL, URLPattern::kAllUrlsPattern),
    user_date(-1),
    suppression(0) {}

DefaultWebIntentService::~DefaultWebIntentService() {}
