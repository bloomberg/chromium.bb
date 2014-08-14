// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_HELP_OVERLAY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_HELP_OVERLAY_HANDLER_H_

#include "base/basictypes.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace options {

// Help UI page handler to support About in Settings when using Settings in a
// window. Defers to ::HelpHandler.
class HelpOverlayHandler : public ::options::OptionsPageUIHandler {
 public:
  HelpOverlayHandler();
  virtual ~HelpOverlayHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(HelpOverlayHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_HELP_OVERLAY_HANDLER_H_
