// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_TRIAL_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_TRIAL_H_

#include "base/basictypes.h"

// These helpers manage the Sync Promo field trial.
//
// The sync promo experiment involves changing the message body in the sign in
// promo (see SyncPromoHandler) to see if that has any effect on sign in.
namespace sync_promo_trial {
  enum Group {
    // "Get your bookmarks, history, and settings on all your devices."
    // This is also the default message group.
    PROMO_MSG_A = 0,

    // "Back up your bookmarks, history, and settings to the web."
    PROMO_MSG_B,

    // "Sync your personalized browser features between all your devices."
    PROMO_MSG_C,

    // "You'll be automatically signed into to your favorite Google services."
    PROMO_MSG_D,

    // Bounding max value needed for UMA histogram macro.
    PROMO_MSG_MAX,
  };

  // Activate the field trial. Before this call, all calls to GetGroup will
  // return PROMO_MSG_A. *** MUST NOT BE CALLED MORE THAN ONCE. ***
  void Activate();

  // Returns true iff the experiment has been set up and is active. If this
  // is false, the caller should not record stats for this experiment.
  bool IsExperimentActive();

  // Return the field trial group this client belongs to.
  Group GetGroup();

  // Return the resource ID for the Sync Promo message body associated with this
  // client.
  int GetMessageBodyResID();

  // Record the appropriate UMA stat for when a user sees the sync promo
  // message.
  void RecordUserSawMessage();

  // Record the appropriate UMA stat for when a user successfully signs in to
  // GAIA.
  void RecordUserSignedIn();
}

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_TRIAL_H_
