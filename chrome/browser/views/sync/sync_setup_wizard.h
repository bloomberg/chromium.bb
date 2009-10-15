// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_SYNC_SYNC_SETUP_WIZARD_H_
#define CHROME_BROWSER_VIEWS_SYNC_SYNC_SETUP_WIZARD_H_

#include "base/basictypes.h"

#if defined(OS_WIN)
class SyncSetupFlowContainer;
#elif defined(OS_LINUX)
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;
#endif

class ProfileSyncService;

class SyncSetupWizard {
 public:
  enum State {
    GAIA_LOGIN = 0,
    GAIA_SUCCESS,
    MERGE_AND_SYNC,
    FATAL_ERROR,
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

#if defined(OS_LINUX)
  void SetVisible(bool visible) { visible_ = visible; }
#endif

 private:
  // If we just need to pop open an individual dialog, say to collect
  // gaia credentials in the event of a steady-state auth failure, this is
  // a "discrete" run (as in not a continuous wizard flow).  This returns
  // the end state to pass to Run for a given |start_state|.
  static State GetEndStateForDiscreteRun(State start_state);

  ProfileSyncService* service_;

#if defined(OS_WIN)
  SyncSetupFlowContainer* flow_container_;
#elif defined(OS_LINUX)
  bool visible_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SyncSetupWizard);
};

#endif  // CHROME_BROWSER_VIEWS_SYNC_SYNC_SETUP_WIZARD_H_
