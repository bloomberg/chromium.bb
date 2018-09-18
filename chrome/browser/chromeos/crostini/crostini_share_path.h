// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/dbus/seneschal/seneschal_service.pb.h"

class Profile;

namespace crostini {

class CrostiniSharePath {
 public:
  // Returns the singleton instance of CrostiniSharePath.
  static CrostiniSharePath* GetInstance();

  // Share specified path with vm.
  void SharePath(Profile* profile,
                 std::string vm_name,
                 std::string path,
                 base::OnceCallback<void(bool, std::string)> callback);
  void OnSharePathResponse(
      std::string path,
      base::OnceCallback<void(bool, std::string)> callback,
      base::Optional<vm_tools::seneschal::SharePathResponse> response) const;

  // Get list of all shared paths for the default crostini container.
  std::vector<std::string> GetSharedPaths(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<CrostiniSharePath>;

  CrostiniSharePath();
  ~CrostiniSharePath();

  base::WeakPtrFactory<CrostiniSharePath> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniSharePath);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_SHARE_PATH_H_
