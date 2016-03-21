// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROFILE_SERVICE_USER_ID_MAP_H_
#define COMPONENTS_PROFILE_SERVICE_USER_ID_MAP_H_

#include <string>
#include "base/files/file_path.h"

namespace profile {

// Currently, ProfileApp is run from within the chrome process. This means that
// the ApplicationLoader is registered during MojoShellContext startup, even
// though the application itself is not started. As soon as a BrowserContext is
// created, the BrowserContext will choose a |user_id| for itself and call us
// to register the mapping from |user_id| to |profile_data_dir|.
//
// This data is then accessed when we get our Initialize() call.
//
// TODO(erg): This is a temporary hack until we redo how we initialize mojo
// applications inside of chrome in general; this system won't work once
// ProfileApp gets put in its own sandboxed process.
void AssociateMojoUserIDWithProfileDir(const std::string& user_id,
                                       const base::FilePath& profile_data_dir);

base::FilePath GetProfileDirForUserID(const std::string& user_id);

}  // namespace profile

#endif  // COMPONENTS_PROFILE_SERVICE_USER_ID_MAP_H_
