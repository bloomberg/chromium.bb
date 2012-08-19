// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_HOME_PAGE_OVERLAY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_HOME_PAGE_OVERLAY_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

class AutocompleteController;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace options {

class HomePageOverlayHandler : public OptionsPageUIHandler,
                               public AutocompleteControllerDelegate {
 public:
  HomePageOverlayHandler();
  virtual ~HomePageOverlayHandler();

  // OptionsPageUIHandler implementation
  virtual void GetLocalizedValues(base::DictionaryValue*) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // AutocompleteControllerDelegate implementation.
  virtual void OnResultChanged(bool default_match_changed) OVERRIDE;

 private:
  void RequestAutocompleteSuggestions(const base::ListValue* args);

  scoped_ptr<AutocompleteController> autocomplete_controller_;

  DISALLOW_COPY_AND_ASSIGN(HomePageOverlayHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_HOME_PAGE_OVERLAY_HANDLER_H_
