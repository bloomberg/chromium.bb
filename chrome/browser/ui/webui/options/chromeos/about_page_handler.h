// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_ABOUT_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_ABOUT_PAGE_HANDLER_H_

#include <string>

#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/chromeos/dbus/update_engine_client.h"
#include "chrome/browser/chromeos/version_loader.h"

namespace chromeos {

// ChromeOS about page UI handler.
class AboutPageHandler : public OptionsPageUIHandler {

 public:
  AboutPageHandler();
  virtual ~AboutPageHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(base::DictionaryValue* localized_strings);
  virtual void RegisterMessages();

 private:
  class UpdateObserver;

  // The function is called from JavaScript when the about page is ready.
  void PageReady(const base::ListValue* args);

  // The function is called from JavaScript to set the release track like
  // "beta-channel" and "dev-channel".
  void SetReleaseTrack(const base::ListValue* args);

  // Initiates update check.
  void CheckNow(const base::ListValue* args);

  // Restarts the system.
  void RestartNow(const base::ListValue* args);

  // Callback from VersionLoader giving the version.
  void OnOSVersion(VersionLoader::Handle handle,
                   std::string version);
  void OnOSFirmware(VersionLoader::Handle handle,
                    std::string firmware);
  void UpdateStatus(const UpdateEngineClient::Status& status);

  // UpdateEngine Callback handler.
  static void UpdateSelectedChannel(UpdateObserver* observer,
                                    const std::string& channel);

  // Handles asynchronously loading the version.
  VersionLoader loader_;

  // Used to request the version.
  CancelableRequestConsumer consumer_;

  // Update Observer
  scoped_ptr<UpdateObserver> update_observer_;

  int progress_;
  bool sticky_;
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(AboutPageHandler);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_ABOUT_PAGE_HANDLER_H_
