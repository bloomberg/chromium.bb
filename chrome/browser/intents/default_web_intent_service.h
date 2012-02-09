// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_DEFAULT_WEB_INTENT_SERVICE_H_
#define CHROME_BROWSER_INTENTS_DEFAULT_WEB_INTENT_SERVICE_H_

#include <string>

#include "base/string16.h"
#include "chrome/common/extensions/url_pattern.h"

// Holds data related to a default settings for web intents service
// selection. Holds data in a row in the web_intents_defaults table.
// The key for the table is the |action|, |type|, and |url_pattern|.
// These describe the action and type of the web intent for which the
// default is set, and the url prefix (if any) of source pages for
// which the default applies. The main actionable value is the URL
// of the service or extension page handling the intent. The |user_date|
// and |suppression| fields are provided for more intricate post-fetch
// decisions about whether to use a particular default service.
struct DefaultWebIntentService {
  string16 action;
  string16 type;
  URLPattern url_pattern;

  // |user_date| holds the offset time when a user set the default.
  // If the user did not set the default explicitly, is <= 0.
  int user_date;

  // |suppression| holds a value modeling whether a default is suppressed.
  // If it has value == 0, the default is active.
  int suppression;

  std::string service_url;

  DefaultWebIntentService();
  ~DefaultWebIntentService();
};

#endif  // CHROME_BROWSER_INTENTS_DEFAULT_WEB_INTENT_SERVICE_H_
