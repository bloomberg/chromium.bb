// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_OPTIONS_UI2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_OPTIONS_UI2_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace options2 {
// The WebUI for chrome:settings-frame.
class OptionsUI : public content::WebUIController,
                  public OptionsPageUIHandlerHost {
 public:
  explicit OptionsUI(content::WebUI* web_ui);
  virtual ~OptionsUI();

  // Takes the suggestions from |autocompleteResult| and adds them to
  // |suggestions| so that they can be passed to a JavaScript function.
  static void ProcessAutocompleteSuggestions(
      const AutocompleteResult& autocompleteResult,
      base::ListValue * const suggestions);

  static RefCountedMemory* GetFaviconResourceBytes();

  // WebUIController implementation.
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewReused(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidBecomeActiveForReusedRenderView() OVERRIDE;

  // Overridden from OptionsPageUIHandlerHost:
  virtual void InitializeHandlers() OVERRIDE;

 private:
  // Adds OptionsPageUiHandler to the handlers list if handler is enabled.
  void AddOptionsPageUIHandler(base::DictionaryValue* localized_strings,
                               OptionsPageUIHandler* handler);

  // Sets the WebUI CommandLineString property with arguments passed while
  // launching chrome.
  void SetCommandLineString(content::RenderViewHost* render_view_host);

  bool initialized_handlers_;

  std::vector<OptionsPageUIHandler*> handlers_;

  DISALLOW_COPY_AND_ASSIGN(OptionsUI);
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_OPTIONS_UI2_H_
