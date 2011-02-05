// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_SETUP_WIZARD_H_
#define CHROME_BROWSER_SYNC_SYNC_SETUP_WIZARD_H_
#pragma once

#include "base/basictypes.h"
#include "ui/gfx/native_widget_types.h"

class SyncSetupFlowContainer;

class ProfileSyncService;

class SyncSetupWizard {
 public:
  enum State {
    // Show the Google Account login UI.
    GAIA_LOGIN = 0,
    // A login attempt succeeded.  This will wait for an explicit transition
    // (via Step) to the next state.
    GAIA_SUCCESS,
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
    // Show the passphrase "first time" screen for migrating users, where all
    // is explained and they choose between google password/custom passphrase.
    PASSPHRASE_MIGRATION,
    // The panic switch.  Something went terribly wrong during setup and we
    // can't recover.
    FATAL_ERROR,
    // The client can't set up sync at the moment due to a concurrent operation
    // to clear cloud data being in progress on the server.
    SETUP_ABORTED_BY_PENDING_CLEAR,
    // Loading screen with spinny throbber.
    SETTING_UP,
    // A final state for when setup completes and it is possible it is the
    // user's first time (globally speaking) as the cloud doesn't have any
    // bookmarks.  We show additional info in this case to explain setting up
    // more computers.
    DONE_FIRST_TIME,
    // A catch-all done case for any setup process.
    DONE
  };

  explicit SyncSetupWizard(ProfileSyncService* service);
  ~SyncSetupWizard();

  // Advances the wizard to the specified state if possible, or opens a
  // new dialog starting at |advance_state|.  If the wizard has never ran
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

  void SetParent(gfx::NativeWindow parent_window);

 private:
  // If we just need to pop open an individual dialog, say to collect
  // gaia credentials in the event of a steady-state auth failure, this is
  // a "discrete" run (as in not a continuous wizard flow).  This returns
  // the end state to pass to Run for a given |start_state|.
  static State GetEndStateForDiscreteRun(State start_state);

  // Helper to return whether |state| warrants starting a new flow.
  static bool IsTerminalState(State state);

  ProfileSyncService* service_;

  SyncSetupFlowContainer* flow_container_;

  gfx::NativeWindow parent_window_;

  DISALLOW_COPY_AND_ASSIGN(SyncSetupWizard);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_SETUP_WIZARD_H_
