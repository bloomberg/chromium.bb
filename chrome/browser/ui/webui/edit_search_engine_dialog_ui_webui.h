// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EDIT_SEARCH_ENGINE_DIALOG_UI_WEBUI_H_
#define CHROME_BROWSER_UI_WEBUI_EDIT_SEARCH_ENGINE_DIALOG_UI_WEBUI_H_
#pragma once

#include "base/values.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"

// The WebUI for chrome://editsearchengine
class EditSearchEngineDialogUI : public HtmlDialogUI {
 public:
  explicit EditSearchEngineDialogUI(WebUI* web_ui);
  virtual ~EditSearchEngineDialogUI();

 protected:
  DISALLOW_COPY_AND_ASSIGN(EditSearchEngineDialogUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EDIT_SEARCH_ENGINE_DIALOG_UI_WEBUI_H_
