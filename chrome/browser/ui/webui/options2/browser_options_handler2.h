// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_BROWSER_OPTIONS_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_BROWSER_OPTIONS_HANDLER2_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"
#include "ui/base/models/table_model_observer.h"

class AutocompleteController;
class CustomHomePagesTableModel;
class TemplateURLService;

namespace options2 {

// Chrome browser options page UI handler.
class BrowserOptionsHandler : public OptionsPageUIHandler,
                              public ProfileSyncServiceObserver,
                              public AutocompleteControllerDelegate,
                              public ShellIntegration::DefaultWebClientObserver,
                              public TemplateURLServiceObserver {
 public:
  BrowserOptionsHandler();
  virtual ~BrowserOptionsHandler();

  virtual void Initialize() OVERRIDE;

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // AutocompleteControllerDelegate implementation.
  virtual void OnResultChanged(bool default_match_changed) OVERRIDE;

  // ShellIntegration::DefaultWebClientObserver implementation.
  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) OVERRIDE;

  // TemplateURLServiceObserver implementation.
  virtual void OnTemplateURLServiceChanged() OVERRIDE;

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Makes this the default browser. Called from WebUI.
  void BecomeDefaultBrowser(const ListValue* args);

  // Sets the search engine at the given index to be default. Called from WebUI.
  void SetDefaultSearchEngine(const ListValue* args);

  // Gets autocomplete suggestions asynchronously for the given string.
  // Called from WebUI.
  void RequestAutocompleteSuggestions(const ListValue* args);

  // Enables/disables Instant.
  void EnableInstant(const ListValue* args);
  void DisableInstant(const ListValue* args);

  // Enables/disables auto-launching of Chrome on computer startup.
  void ToggleAutoLaunch(const ListValue* args);

  // Checks (on the file thread) whether the user is in the auto-launch trial
  // and whether Chrome is set to auto-launch at login. Gets a reply on the UI
  // thread (see CheckAutoLaunchCallback). A weak pointer to this is passed in
  // as a parameter to avoid the need to lock between this function and the
  // destructor.

  void CheckAutoLaunch(base::WeakPtr<BrowserOptionsHandler> weak_this);
  // Sets up (on the UI thread) the necessary bindings for toggling auto-launch
  // (if the user is part of the auto-launch and makes sure the HTML UI knows
  // whether Chrome will auto-launch at login.
  void CheckAutoLaunchCallback(bool is_in_auto_launch_group,
                               bool will_launch_at_login);

  // Called to request information about the Instant field trial.
  void GetInstantFieldTrialStatus(const ListValue* args);

  // Returns the string ID for the given default browser state.
  int StatusStringIdForState(ShellIntegration::DefaultWebClientState state);

  // Gets the current default browser state, and asynchronously reports it to
  // the WebUI page.
  void UpdateDefaultBrowserState();

  // Updates the UI with the given state for the default browser.
  void SetDefaultBrowserUIString(int status_string_id);

  // Updates the label of the 'Show Home page'.
  void UpdateHomePageLabel() const;

  // Loads the possible default search engine list and reports it to the WebUI.
  void UpdateSearchEngines();

  // Sends an array of Profile objects to javascript.
  // Each object is of the form:
  //   profileInfo = {
  //     name: "Profile Name",
  //     iconURL: "chrome://path/to/icon/image",
  //     filePath: "/path/to/profile/data/on/disk",
  //     isCurrentProfile: false
  //   };
  void SendProfilesInfo();

  // Asynchronously opens a new browser window to create a new profile.
  // |args| is not used.
  void CreateProfile(const ListValue* args);

  scoped_refptr<ShellIntegration::DefaultBrowserWorker>
      default_browser_worker_;

  StringPrefMember homepage_;
  BooleanPrefMember default_browser_policy_;

  // Used to observe updates to the preference of the list of URLs to load
  // on startup, which can be updated via sync.
  PrefChangeRegistrar pref_change_registrar_;

  TemplateURLService* template_url_service_;  // Weak.

  scoped_ptr<AutocompleteController> autocomplete_controller_;

  // Used to get |weak_ptr_| to self for use on the File thread.
  base::WeakPtrFactory<BrowserOptionsHandler> weak_ptr_factory_for_file_;
  // Used to post update tasks to the UI thread.
  base::WeakPtrFactory<BrowserOptionsHandler> weak_ptr_factory_for_ui_;

  // True if the multiprofiles switch is enabled.
  bool multiprofile_;

  DISALLOW_COPY_AND_ASSIGN(BrowserOptionsHandler);
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_BROWSER_OPTIONS_HANDLER2_H_
