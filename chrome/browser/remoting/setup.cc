// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/setup.h"
#include "chrome/browser/remoting/setup_flow.h"

//#include "app/l10n_util.h"
//#include "base/utf_string_conversions.h"
#include "chrome/browser/google_service_auth_error.h"
#include "chrome/browser/profile.h"

typedef GoogleServiceAuthError AuthError;

namespace remoting_setup {

void OpenRemotingSetupDialog(Profile* profile) {
    remoting_setup::SetupFlow::Run(profile->GetOriginalProfile());
}

}  // namespace sync_ui_util
