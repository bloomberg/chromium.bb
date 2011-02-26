// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_ABOUT_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_ABOUT_PAGE_HANDLER_H_

#include <string>

#include "chrome/browser/ui/webui/options/options_ui.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/update_library.h"
#include "chrome/browser/chromeos/version_loader.h"
#endif

// ChromeOS about page UI handler.
class AboutPageHandler : public OptionsPageUIHandler {
 public:
  AboutPageHandler();
  virtual ~AboutPageHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void RegisterMessages();

 private:
  // The function is called from JavaScript when the about page is ready.
  void PageReady(const ListValue* args);

  // The function is called from JavaScript to set the release track like
  // "beta-channel" and "dev-channel".
  void SetReleaseTrack(const ListValue* args);

#if defined(OS_CHROMEOS)
  // Initiates update check.
  void CheckNow(const ListValue* args);

  // Restarts the system.
  void RestartNow(const ListValue* args);

  // Callback from chromeos::VersionLoader giving the version.
  void OnOSVersion(chromeos::VersionLoader::Handle handle,
                   std::string version);
  void UpdateStatus(const chromeos::UpdateLibrary::Status& status);

  // Handles asynchronously loading the version.
  chromeos::VersionLoader loader_;

  // Used to request the version.
  CancelableRequestConsumer consumer_;

  // Update Observer
  class UpdateObserver;
  scoped_ptr<UpdateObserver> update_observer_;

  int progress_;
  bool sticky_;
  bool started_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AboutPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_ABOUT_PAGE_HANDLER_H_
