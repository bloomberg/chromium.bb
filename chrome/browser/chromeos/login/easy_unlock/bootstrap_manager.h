// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_BOOTSTRAP_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_BOOTSTRAP_MANAGER_H_

#include <string>

#include "base/macros.h"

class PrefRegistrySimple;

namespace chromeos {

// BootstrapManager manages bootstrap user specific values such as tracking
// pending bootstrap and clearing such state.
class BootstrapManager {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  class Delegate {
   public:
    virtual void RemovePendingBootstrapUser(const std::string& user_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit BootstrapManager(Delegate* delegate);
  ~BootstrapManager();

  void AddPendingBootstrap(const std::string& user_id);
  void FinishPendingBootstrap(const std::string& user_id);
  void RemoveAllPendingBootstrap();

  bool HasPendingBootstrap(const std::string& user_id) const;

 private:
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(BootstrapManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_BOOTSTRAP_MANAGER_H_
