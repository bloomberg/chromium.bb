// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chromeos/dbus/seneschal/seneschal_service.pb.h"

class Profile;

namespace crostini {

// Share specified absolute path with vm.
// Callback receives success bool and failure reason string.
void SharePath(Profile* profile,
               std::string vm_name,
               const base::FilePath& path,
               base::OnceCallback<void(bool, std::string)> callback);

// Get list of all shared paths for the default crostini container.
std::vector<std::string> GetSharedPaths(Profile* profile);

// Share all paths configured in prefs for the default crostini container.
// Called at container startup.  Callback is invoked once complete.
void ShareAllPaths(Profile* profile, base::OnceCallback<void()> callback);

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_H_
