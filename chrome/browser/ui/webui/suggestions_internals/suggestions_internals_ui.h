// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUGGESTIONS_INTERNALS_SUGGESTIONS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SUGGESTIONS_INTERNALS_SUGGESTIONS_INTERNALS_UI_H_

#include "base/basictypes.h"
#include "content/public/browser/web_ui_controller.h"

// The UI for chrome://suggestions-internals/
class SuggestionsInternalsUI : public content::WebUIController {
 public:
  explicit SuggestionsInternalsUI(content::WebUI* contents);
  virtual ~SuggestionsInternalsUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(SuggestionsInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SUGGESTIONS_INTERNALS_SUGGESTIONS_INTERNALS_UI_H_
