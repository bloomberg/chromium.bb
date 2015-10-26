// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_FONT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_FONT_HANDLER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/md_settings_ui.h"

namespace base {
class ListValue;
}

namespace content {
class WebUI;
}

namespace settings {

// Handle OS font list, font preference settings and character encoding.
class FontHandler : public SettingsPageUIHandler {
 public:
  explicit FontHandler(content::WebUI* webui);
  ~FontHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;

 private:
  // Handler for script asking for font information.
  void HandleFetchFontsData(const base::ListValue* args);

  // Callback to handle fonts loading.
  void FontListHasLoaded(scoped_ptr<base::ListValue> list);

  base::WeakPtrFactory<FontHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FontHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_FONT_HANDLER_H_
