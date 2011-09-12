// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_SETUP_FLOW_H_
#define CHROME_BROWSER_SYNC_SYNC_SETUP_FLOW_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_wizard.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

class SyncSetupFlowHandler;
class SyncSetupFlowContainer;

// A structure which contains all the configuration information for sync.
// This can be stored or passed around when the configuration is managed
// by multiple stages of the wizard.
struct SyncConfiguration {
  SyncConfiguration();
  ~SyncConfiguration();

  bool encrypt_all;
  bool sync_everything;
  syncable::ModelTypeSet data_types;
  // We pass a separate |set_xxxxx_passphrase| flag because sometimes the UI
  // wants to set an empty gaia/secondary passphrase (for example, when the user
  // doesn't enter a passphrase, but we still want the ProfileSyncService to
  // generate a new passphrase error if there are still encrypted types
  // enabled).
  // TODO(atwilson): Need to change SyncSetupFlow::OnUserConfigured() to
  // check for the presence of encrypted types itself, rather than relying on
  // the hack of passing an empty passphrase/waiting for ProfileSyncService to
  // receive a new PassphraseRequired (http://crbug.com/95939).
  bool set_secondary_passphrase;
  std::string secondary_passphrase;
  bool set_gaia_passphrase;
  std::string gaia_passphrase;
};

// The state machine used by SyncSetupWizard, exposed in its own header
// to facilitate testing of SyncSetupWizard.  This class is used to open and
// run the sync setup overlay in tabbed options and deletes itself when the
// overlay closes.
class SyncSetupFlow {
 public:
  virtual ~SyncSetupFlow();

  // Runs a flow from |start| to |end|, and does the work of actually showing
  // the HTML dialog.  |container| is kept up-to-date with the lifetime of the
  // flow (e.g it is emptied on dialog close).
  static SyncSetupFlow* Run(ProfileSyncService* service,
                            SyncSetupFlowContainer* container,
                            SyncSetupWizard::State start,
                            SyncSetupWizard::State end);

  // Fills |args| with "user" and "error" arguments by querying |service|.
  static void GetArgsForGaiaLogin(
      const ProfileSyncService* service,
      DictionaryValue* args);

  // Fills |args| for the configure screen (Choose Data Types/Encryption)
  void GetArgsForConfigure(ProfileSyncService* service, DictionaryValue* args);

  // Attaches the |handler| to this flow. Returns true if successful and false
  // if a handler has already been attached.
  bool AttachSyncSetupHandler(SyncSetupFlowHandler* handler);

  // Returns true if a handler is attached to this flow.
  bool IsAttached() const;

  // Triggers a state machine transition to advance_state.
  void Advance(SyncSetupWizard::State advance_state);

  // Focuses the dialog.  This is useful in cases where the dialog has been
  // obscured by a browser window.
  void Focus();

  void OnUserSubmittedAuth(const std::string& username,
                           const std::string& password,
                           const std::string& captcha,
                           const std::string& access_code);

  void OnUserSubmittedOAuth(const std::string& oauth1_request_token);

  void OnUserConfigured(const SyncConfiguration& configuration);

  // The 'passphrase' screen is used when the user is prompted to enter
  // an existing passphrase.
  void OnPassphraseEntry(const std::string& passphrase);

  // The user canceled the passphrase entry without supplying a passphrase.
  void OnPassphraseCancel();

  void OnDialogClosed(const std::string& json_retval);

 private:
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, InitialStepLogin);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, ChooseDataTypesSetsPrefs);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, DialogCancelled);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, InvalidTransitions);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, FullSuccessfulRunSetsPref);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, AbortedByPendingClear);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, DiscreteRunGaiaLogin);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, DiscreteRunChooseDataTypes);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest,
                           DiscreteRunChooseDataTypesAbortedByPendingClear);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, EnterPassphraseRequired);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, NonFatalError);
  FRIEND_TEST_ALL_PREFIXES(SyncSetupWizardTest, CrosAuthSetup);

  // Use static Run method to get an instance.
  SyncSetupFlow(SyncSetupWizard::State start_state,
                SyncSetupWizard::State end_state,
                SyncSetupFlowContainer* container,
                ProfileSyncService* service);

  // Returns true if |this| should transition its state machine to |state|
  // based on |current_state_|, or false if that would be nonsense or is
  // a no-op.
  bool ShouldAdvance(SyncSetupWizard::State state);

  void ActivateState(SyncSetupWizard::State state);

  SyncSetupFlowContainer* container_;  // Our container.  Don't own this.

  SyncSetupWizard::State current_state_;
  SyncSetupWizard::State end_state_;  // The goal.

  // Time that the GAIA_LOGIN step was received.
  base::TimeTicks login_start_time_;

  // The handler needed for the entire flow. Weak reference.
  SyncSetupFlowHandler* flow_handler_;

  // We need this to propagate back all user settings changes. Weak reference.
  ProfileSyncService* service_;

  // Set to true if we've tried creating an explicit passphrase, so we
  // can appropriately reflect this in the UI.
  bool tried_creating_explicit_passphrase_;

  // Set to true if the user entered a passphrase, so we can appropriately
  // reflect this in the UI.
  bool tried_setting_passphrase_;

  // We track the passphrase the user entered so we can set it when configuring
  // the ProfileSyncService.
  std::string cached_passphrase_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupFlow);
};

// A really simple wrapper for a SyncSetupFlow so that we don't have to
// add any public methods to the public SyncSetupWizard interface to notify it
// when the dialog closes.
class SyncSetupFlowContainer {
 public:
  SyncSetupFlowContainer() : flow_(NULL) { }
  void set_flow(SyncSetupFlow* flow) {
    DCHECK(!flow_ || !flow);
    flow_ = flow;
  }

  SyncSetupFlow* get_flow() { return flow_; }
 private:
  SyncSetupFlow* flow_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupFlowContainer);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_SETUP_FLOW_H_
