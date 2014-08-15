// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_REMOVE_USER_DELEGATE_H_
#define COMPONENTS_USER_MANAGER_REMOVE_USER_DELEGATE_H_

#include "components/user_manager/user_manager_export.h"

namespace user_manager {

// Delegate to be used with |UserManager::RemoveUser|.
class USER_MANAGER_EXPORT RemoveUserDelegate {
 public:
  // Called right before actual user removal process is initiated.
  virtual void OnBeforeUserRemoved(const std::string& username) = 0;

  // Called right after user removal process has been initiated.
  virtual void OnUserRemoved(const std::string& username) = 0;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_REMOVE_USER_DELEGATE_H_
