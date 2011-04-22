// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_setup_wizard.h"

#include <stddef.h>
#include <ostream>

#include "base/logging.h"
#include "chrome/browser/sync/sync_setup_flow.h"

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
    flow_container_->set_flow(
        SyncSetupFlow::Run(service_, flow_container_, advance_state, DONE));
  } else {
    // No flow in progress, but we've finished the wizard flow once before.
    // This is just a discrete run.
    if (IsTerminalState(advance_state))
      return;  // Nothing to do.
    flow_container_->set_flow(SyncSetupFlow::Run(service_, flow_container_,
        advance_state, GetEndStateForDiscreteRun(advance_state)));
  }
}

// static
bool SyncSetupWizard::IsTerminalState(State advance_state) {
  return advance_state == GAIA_SUCCESS ||
         advance_state == DONE ||
         advance_state == DONE_FIRST_TIME ||
         advance_state == FATAL_ERROR ||
         advance_state == SETUP_ABORTED_BY_PENDING_CLEAR;
}

bool SyncSetupWizard::IsVisible() const {
  return flow_container_->get_flow() != NULL;
}

void SyncSetupWizard::Focus() {
  SyncSetupFlow* flow = flow_container_->get_flow();
  if (flow) {
    flow->Focus();
  }
}

SyncSetupFlow* SyncSetupWizard::AttachSyncSetupHandler(
    SyncSetupFlowHandler* handler) {
  SyncSetupFlow* flow = flow_container_->get_flow();
  if (!flow)
    return NULL;

  flow->AttachSyncSetupHandler(handler);
  return flow;
}

// static
SyncSetupWizard::State SyncSetupWizard::GetEndStateForDiscreteRun(
    State start_state) {
  State result = FATAL_ERROR;
  if (start_state == GAIA_LOGIN) {
    result = GAIA_SUCCESS;
  } else if (start_state == ENTER_PASSPHRASE ||
             start_state == SYNC_EVERYTHING ||
             start_state == CONFIGURE ||
             start_state == PASSPHRASE_MIGRATION) {
    result = DONE;
  }
  DCHECK_NE(FATAL_ERROR, result) <<
      "Invalid start state for discrete run: " << start_state;
  return result;
}
