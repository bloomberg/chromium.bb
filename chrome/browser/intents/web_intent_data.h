// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_WEB_INTENT_DATA_H_
#define CHROME_BROWSER_INTENTS_WEB_INTENT_DATA_H_
#pragma once

#include "base/string16.h"
#include "googleurl/src/gurl.h"

// Describes the relevant elements of a WebIntent.
struct WebIntentData {
  GURL service_url;  // URL for service invocation.
  string16 action;  // Name of action provided by service.
  string16 type;  // MIME type of data accepted by service.
};

#endif  // CHROME_BROWSER_INTENTS_WEB_INTENT_DATA_H_
