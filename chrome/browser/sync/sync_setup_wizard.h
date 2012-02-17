// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_SETUP_WIZARD_H_
#define CHROME_BROWSER_SYNC_SYNC_SETUP_WIZARD_H_
#pragma once

#include "base/basictypes.h"

class SyncSetupFlow;
class SyncSetupFlowContainer;
class SyncSetupFlowHandler;

class ProfileSyncService;

class SyncSetupWizard {
 public:
  enum State {
    SYNC_EVERYTHING,
    // Show the screen that lets you configure sync.
    // There are two tabs:
    //  Data Types --
    //   Choose either "Keep everything synced" or
    //   "Choose which data types to sync", and checkboxes for each data type.
    //  Encryption --
    //   Choose what to encrypt and whether to use a passphrase.
    CONFIGURE,
    // Show the screen that prompts for your passphrase
    ENTER_PASSPHRASE,
    // The panic switch. Something went terribly wrong during setup and we can't
    // recover.
    FATAL_ERROR,
    // Loading screen with throbber.
    SETTING_UP,
    // A catch-all done case for any setup process.
    DONE,
    // Exit the wizard (for example, if the user clears the data on the
    // dashboard while the wizard is open).
    ABORT,
  };

  explicit SyncSetupWizard(ProfileSyncService* service);
  ~SyncSetupWizard();

  // Advances the wizard to the specified state if possible, or opens a
  // new dialog starting at |advance_state|.  If the wizard has never run
  // through to completion, it will always attempt to do so.  Otherwise, e.g
  // for a transient auth failure, it will just run as far as is necessary
  // based on |advance_state| (so for auth failure, up to GAIA_SUCCESS).
  void Step(State advance_state);

  // Whether or not a dialog is currently showing.  Useful to determine
  // if various buttons in the UI should be enabled or disabled.
  bool IsVisible() const;

  // Focus the dialog if it is already open.  Does nothing if the dialog is
  // not visible.
  void Focus();

  // Attaches |handler| to the flow contained in |flow_container_|. Returns NULL
  // if the flow does not exist or if an existing handler is already attached to
  // the flow.
  SyncSetupFlow* AttachSyncSetupHandler(SyncSetupFlowHandler* handler);

 private:
  ProfileSyncService* service_;

  SyncSetupFlowContainer* flow_container_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupWizard);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_SETUP_WIZARD_H_
