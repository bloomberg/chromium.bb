// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_DELEGATE_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_DELEGATE_H_

#include <string>
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class WebContents;
}

class Profile;

// A class used to notify the delegate when the user has chosen a web intent
// service.
class WebIntentPickerDelegate {
 public:
  enum DefaultsUsage {
    kEnableDefaults = 0,
    kSuppressDefaults = 1,
  };

  // Base destructor.
  virtual ~WebIntentPickerDelegate() {}

  // Called when the user has chosen a service.
  virtual void OnServiceChosen(
      const GURL& url,
      webkit_glue::WebIntentServiceData::Disposition disposition,
      DefaultsUsage suppress_defaults) = 0;

  // Called to create the WebContents into which the inline disposition will be
  // placed.
  virtual content::WebContents* CreateWebContentsForInlineDisposition(
      Profile* profile, const GURL& url) = 0;

  // Called when the user has chosen to install a suggested extension.
  virtual void OnExtensionInstallRequested(const std::string& id) = 0;

  // Called when the user has chosen to visit the CWS entry for an extension.
  // |id| is the extension id.
  // |disposition| is user-requested disposition for opening the extension
  // link.
  virtual void OnExtensionLinkClicked(
      const std::string& id,
      WindowOpenDisposition disposition) = 0;

  // Called when the user chooses to get more suggestions from CWS.
  // |disposition| is user-requested disposition for opening the suggestions
  // link.
  virtual void OnSuggestionsLinkClicked(WindowOpenDisposition disposition) = 0;

  // Called when the user cancels out of the dialog.
  virtual void OnUserCancelledPickerDialog() = 0;

  // Called when the user wants to pick another service from within inline
  // disposition.
  virtual void OnChooseAnotherService() = 0;

  // Called when the dialog stops showing.
  virtual void OnClosing() = 0;
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_DELEGATE_H_
