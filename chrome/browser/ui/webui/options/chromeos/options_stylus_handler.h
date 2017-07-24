// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_OPTIONS_STYLUS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_OPTIONS_STYLUS_HANDLER_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "ui/events/devices/input_device_event_observer.h"

class Profile;

namespace base {
class ListValue;
}  // namespace base

namespace chromeos {
namespace options {

// Stylus-specific options C++ code.
class OptionsStylusHandler : public ::options::OptionsPageUIHandler,
                             public NoteTakingHelper::Observer,
                             public ui::InputDeviceEventObserver {
 public:
  OptionsStylusHandler();
  ~OptionsStylusHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializePage() override;
  void RegisterMessages() override;

  // NoteTakingHelper::Observer implementation.
  void OnAvailableNoteTakingAppsUpdated() override;
  void OnPreferredNoteTakingAppUpdated(Profile* profile) override;

  // ui::InputDeviceEventObserver implementation:
  void OnDeviceListsComplete() override;

 private:
  // Called from WebUI when the page is initialized to determine if stylus
  // settings should be shown.
  void RequestStylusHardwareState(const base::ListValue* args);

  // Sends if there is a stylus device to WebUI.
  void SendHasStylus();

  // Updates the note-taking app menu in the stylus overlay to display the
  // currently-available set of apps.
  void UpdateNoteTakingApps();

  // Called from WebUI when the user selects a note-taking app.
  void SetPreferredNoteTakingApp(const base::ListValue* args);

  // IDs of available note-taking apps.
  std::set<std::string> note_taking_app_ids_;

  base::WeakPtrFactory<OptionsStylusHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OptionsStylusHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_OPTIONS_STYLUS_HANDLER_H_
