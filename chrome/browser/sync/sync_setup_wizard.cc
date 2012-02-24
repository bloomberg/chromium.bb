// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

// Helper to return whether |state| warrants starting a new flow.
bool IsTerminalState(SyncSetupWizard::State state) {
  return state == SyncSetupWizard::DONE ||
         state == SyncSetupWizard::FATAL_ERROR;
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
  } else {
    if (IsTerminalState(advance_state))
      return;
    // Starting a new flow - make sure we're at a valid starting state.
    DCHECK(advance_state == ENTER_PASSPHRASE ||
           advance_state == SYNC_EVERYTHING ||
           advance_state == CONFIGURE);
    flow_container_->set_flow(
        SyncSetupFlow::Run(service_, flow_container_, advance_state, DONE));
  }
}

bool SyncSetupWizard::IsVisible() const {
  return flow_container_->get_flow() != NULL &&
         flow_container_->get_flow()->IsAttached();
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
