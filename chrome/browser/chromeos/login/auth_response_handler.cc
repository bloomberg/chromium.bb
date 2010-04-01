// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth_response_handler.h"

namespace chromeos {

const int kHttpSuccess = 200;
const char AuthResponseHandler::kClientLoginUrl[] =
    "https://www.google.com/accounts/ClientLogin";
const char AuthResponseHandler::kIssueAuthTokenUrl[] =
    "https://www.google.com/accounts/IssueAuthToken";
// TODO(cmasone): make sure that using an http:// URL in the "continue"
// parameter here doesn't open the system up to attack long-term.
const char AuthResponseHandler::kTokenAuthUrl[] =
    "https://www.google.com/accounts/TokenAuth?"
    "continue=http://www.google.com/webhp&source=chromeos&auth=";

}  // namespace chromeos
