// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REMOTING_SETUP_H_
#define CHROME_BROWSER_REMOTING_SETUP_H_

#include "base/string16.h"

class Profile;

namespace remoting_setup {

// Open the appropriate setup dialog for the given profile (which can be
// incognito).
void OpenRemotingSetupDialog(Profile* profile);
}  // namespace remoting_setup

#endif  // CHROME_BROWSER_REMOTING_SETUP_H_
