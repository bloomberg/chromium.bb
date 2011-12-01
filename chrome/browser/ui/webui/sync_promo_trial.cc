// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo_trial.h"

#include "base/metrics/field_trial.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "grit/generated_resources.h"

namespace sync_promo_trial {

// Field trial IDs of the control and experiment groups. Though they are not
// literally "const", they are set only once, in Activate() below. See the .h
// file for what these groups represent.
int g_sync_promo_experiment_a = 0;
int g_sync_promo_experiment_b = 0;
int g_sync_promo_experiment_c = 0;
int g_sync_promo_experiment_d = 0;

const char kSyncPromoMsgTrialName[] = "SyncPromoMsg";

void Activate() {
  // The end date (February 21, 2012) is approximately 2 weeks into M17 stable.
  scoped_refptr<base::FieldTrial> trial(
      new base::FieldTrial(kSyncPromoMsgTrialName, 1000, "MsgA", 2012, 2, 21));
  g_sync_promo_experiment_a = base::FieldTrial::kDefaultGroupNumber;

  // Try to give the user a consistent experience, if possible.
  if (base::FieldTrialList::IsOneTimeRandomizationEnabled())
    trial->UseOneTimeRandomization();

  // Each group has probability 0.5%, leaving the control with 98.5%.
  g_sync_promo_experiment_b = trial->AppendGroup("MsgB", 50);
  g_sync_promo_experiment_c = trial->AppendGroup("MsgC", 50);
  g_sync_promo_experiment_d = trial->AppendGroup("MsgD", 50);
}

bool IsExperimentActive() {
  return base::FieldTrialList::FindValue(kSyncPromoMsgTrialName) !=
      base::FieldTrial::kNotFinalized;
}

Group GetGroup() {
  // Promo message A is also the default value, so display it if there is no
  // active experiment.
  if (!IsExperimentActive())
    return PROMO_MSG_A;

  const int group = base::FieldTrialList::FindValue(kSyncPromoMsgTrialName);
  if (group == g_sync_promo_experiment_a)
    return PROMO_MSG_A;
  else if (group == g_sync_promo_experiment_b)
    return PROMO_MSG_B;
  else if (group == g_sync_promo_experiment_c)
    return PROMO_MSG_C;
  else if (group == g_sync_promo_experiment_d)
    return PROMO_MSG_D;

  NOTREACHED();
  return PROMO_MSG_A;
}

int GetMessageBodyResID() {
  // Note that GetGroup and the switch will take care of the !IsExperimentActive
  // case for us.
  Group group = GetGroup();
  switch (group) {
    case PROMO_MSG_A:
      return IDS_SYNC_PROMO_MESSAGE_BODY_A;
    case PROMO_MSG_B:
      return IDS_SYNC_PROMO_MESSAGE_BODY_B;
    case PROMO_MSG_C:
      return IDS_SYNC_PROMO_MESSAGE_BODY_C;
    case PROMO_MSG_D:
      return IDS_SYNC_PROMO_MESSAGE_BODY_D;
    case PROMO_MSG_MAX:
      break;
  }

  NOTREACHED();
  return IDS_SYNC_PROMO_MESSAGE_BODY_A;
}

void RecordUserSawMessage() {
  DCHECK(IsExperimentActive());
  UMA_HISTOGRAM_ENUMERATION("SyncPromo.MessageDisplayed",
                            GetGroup(),
                            PROMO_MSG_MAX);
}

void RecordUserSignedIn() {
  DCHECK(IsExperimentActive());
  UMA_HISTOGRAM_ENUMERATION("SyncPromo.MessageOnSignIn",
                            GetGroup(),
                            PROMO_MSG_MAX);
}

}  // namespace sync_promo_trial
