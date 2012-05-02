// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUGGESTIONS_INTERNALS_SUGGESTIONS_INTERNALS_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SUGGESTIONS_INTERNALS_SUGGESTIONS_INTERNALS_UI_HANDLER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

// UI Handler for chrome://suggestions-internals/
class SuggestionsInternalsUIHandler : public content::WebUIMessageHandler {
 public:
  explicit SuggestionsInternalsUIHandler(Profile* profile);
  virtual ~SuggestionsInternalsUIHandler();

 protected:
  // WebUIMessageHandler implementation.
  // Register our handler to get callbacks from javascript.
  virtual void RegisterMessages() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsInternalsUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SUGGESTIONS_INTERNALS_SUGGESTIONS_INTERNALS_UI_HANDLER_H_
