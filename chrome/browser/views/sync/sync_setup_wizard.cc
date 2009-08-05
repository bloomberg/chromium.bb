// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include "chrome/browser/views/sync/sync_setup_wizard.h"

#include "chrome/common/pref_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/views/sync/sync_setup_flow.h"

SyncSetupWizard::SyncSetupWizard(ProfileSyncService* service)
      : service_(service), flow_container_(new SyncSetupFlowContainer()) {
}

SyncSetupWizard::~SyncSetupWizard() {
  delete flow_container_;
}

void SyncSetupWizard::Step(State advance_state) {
  SyncSetupFlow* flow = flow_container_->get_flow();
  if (flow) {
    // A setup flow is in progress and dialog is currently showing.
    flow->Advance(advance_state);
  } else if (!service_->profile()->GetPrefs()->GetBoolean(
             prefs::kSyncHasSetupCompleted)) {
    if (advance_state == DONE || advance_state == GAIA_SUCCESS)
      return;
    // No flow is in progress, and we have never escorted the user all the
    // way through the wizard flow.
    flow_container_->set_flow(
        SyncSetupFlow::Run(service_, flow_container_, advance_state, DONE));
  } else {
    // No flow in in progress, but we've finished the wizard flow once before.
    // This is just a discrete run.
    if (advance_state == DONE || advance_state == GAIA_SUCCESS)
      return;  // Nothing to do.
    flow_container_->set_flow(SyncSetupFlow::Run(service_, flow_container_,
        advance_state, GetEndStateForDiscreteRun(advance_state)));
  }
}

bool SyncSetupWizard::IsVisible() const {
  return flow_container_->get_flow() != NULL;
}

// static
SyncSetupWizard::State SyncSetupWizard::GetEndStateForDiscreteRun(
    State start_state) {
  State result = start_state == GAIA_LOGIN ? GAIA_SUCCESS : DONE;
  DCHECK_NE(DONE, result) <<
      "Invalid start state for discrete run: " << start_state;
  return result;
}

#endif  // CHROME_PERSONALIZATION
