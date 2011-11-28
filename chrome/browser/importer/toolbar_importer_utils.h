// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_TOOLBAR_IMPORTER_UTILS_H_
#define CHROME_BROWSER_IMPORTER_TOOLBAR_IMPORTER_UTILS_H_
#pragma once

#include "base/callback_forward.h"

class Profile;

namespace toolbar_importer_utils {

// Currently the only configuration information we need is to check whether or
// not the user currently has their GAIA cookie.  This is done by the function
// exposed through the ToolbarImportUtils namespace.
void IsGoogleGAIACookieInstalled(const base::Callback<void(bool)>& callback,
                                 Profile* profile);

}  // namespace toolbar_importer_utils

#endif  // CHROME_BROWSER_IMPORTER_TOOLBAR_IMPORTER_UTILS_H_
