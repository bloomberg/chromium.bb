// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_setup_wizard.h"

#include <stddef.h>
#include <ostream>

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

namespace {

// If we just need to pop open an individual dialog, say to collect
// gaia credentials in the event of a steady-state auth failure, this is
// a "discrete" run (as in not a continuous wizard flow).  This returns
// the end state to pass to Run for a given |start_state|.
SyncSetupWizard::State GetEndStateForDiscreteRun(
    SyncSetupWizard::State start_state) {
  SyncSetupWizard::State result = SyncSetupWizard::FATAL_ERROR;
  if (start_state == SyncSetupWizard::GAIA_LOGIN ||
      start_state == SyncSetupWizard::OAUTH_LOGIN) {
    result = SyncSetupWizard::GAIA_SUCCESS;
  } else if (start_state == SyncSetupWizard::ENTER_PASSPHRASE ||
             start_state == SyncSetupWizard::NONFATAL_ERROR ||
             start_state == SyncSetupWizard::SYNC_EVERYTHING ||
             start_state == SyncSetupWizard::CONFIGURE) {
    result = SyncSetupWizard::DONE;
  }
  DCHECK_NE(SyncSetupWizard::FATAL_ERROR, result) <<
      "Invalid start state for discrete run: " << start_state;
  return result;
}

// Helper to return whether |state| warrants starting a new flow.
bool IsTerminalState(SyncSetupWizard::State state) {
  return state == SyncSetupWizard::GAIA_SUCCESS ||
         state == SyncSetupWizard::DONE ||
         state == SyncSetupWizard::FATAL_ERROR ||
         state == SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR;
}

}  // namespace

SyncSetupWizard::SyncSetupWizard(ProfileSyncService* service)
    : service_(service),
      flow_container_(new SyncSetupFlowContainer()) {
}

SyncSetupWizard::~SyncSetupWizard() {
  delete flow_container_;
}

void SyncSetupWizard::Step(State advance_state) {
  SyncSetupFlow* flow = flow_container_->get_flow();
  if (flow) {
    // A setup flow is in progress and dialog is currently showing.
    flow->Advance(advance_state);
  } else if (!service_->HasSyncSetupCompleted()) {
    if (IsTerminalState(advance_state))
      return;
    // No flow is in progress, and we have never escorted the user all the
    // way through the wizard flow.
    State end_state = DONE;
    if (!service_->cros_user().empty() &&
        !service_->profile()->GetPrefs()->GetBoolean(
            prefs::kSyncSuppressStart)) {
      end_state = GAIA_SUCCESS;
    }
    flow_container_->set_flow(
        SyncSetupFlow::Run(service_, flow_container_, advance_state,
                           end_state));
  } else {
    // No flow in progress, but we've finished the wizard flow once before.
    // This is just a discrete run.
    if (IsTerminalState(advance_state))
      return;
    flow_container_->set_flow(SyncSetupFlow::Run(service_, flow_container_,
        advance_state, GetEndStateForDiscreteRun(advance_state)));
  }
}

bool SyncSetupWizard::IsVisible() const {
  return flow_container_->get_flow() != NULL;
}

// static
SyncSetupWizard::State SyncSetupWizard::GetLoginState() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableSyncOAuth))
    return SyncSetupWizard::OAUTH_LOGIN;
  return SyncSetupWizard::GAIA_LOGIN;
}

void SyncSetupWizard::Focus() {
  SyncSetupFlow* flow = flow_container_->get_flow();
  if (flow)
    flow->Focus();
}

SyncSetupFlow* SyncSetupWizard::AttachSyncSetupHandler(
    SyncSetupFlowHandler* handler) {
  SyncSetupFlow* flow = flow_container_->get_flow();
  if (!flow || !flow->AttachSyncSetupHandler(handler))
    return NULL;

  return flow;
}
