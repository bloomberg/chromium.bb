// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_dice_internals_handler.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"

SigninDiceInternalsHandler::SigninDiceInternalsHandler(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  DCHECK(!profile_->IsOffTheRecord());
}

SigninDiceInternalsHandler::~SigninDiceInternalsHandler() {}

void SigninDiceInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "enableSync", base::Bind(&SigninDiceInternalsHandler::HandleEnableSync,
                               base::Unretained(this)));
}

void SigninDiceInternalsHandler::HandleEnableSync(const base::ListValue* args) {
  // TODO(msarda): Implement start syncing.
  VLOG(1) << "[Dice] Start syncing";
}
