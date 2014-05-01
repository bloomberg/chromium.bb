// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_STUB_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_STUB_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"

namespace chrome {

// This is the implementation of MultiUserWindowManager for single user mode.
class MultiUserWindowManagerStub : public MultiUserWindowManager {
 public:
  MultiUserWindowManagerStub() {}
  virtual ~MultiUserWindowManagerStub() {}

  // MultiUserWindowManager overrides:
  virtual void SetWindowOwner(
      aura::Window* window, const std::string& user_id) OVERRIDE;
  virtual const std::string& GetWindowOwner(
      aura::Window* window) const OVERRIDE;
  virtual void ShowWindowForUser(
      aura::Window* window, const std::string& user_id) OVERRIDE;
  virtual bool AreWindowsSharedAmongUsers() const OVERRIDE;
  virtual void GetOwnersOfVisibleWindows(
      std::set<std::string>* user_ids) const OVERRIDE;
  virtual bool IsWindowOnDesktopOfUser(
      aura::Window* window,
      const std::string& user_id) const OVERRIDE;
  virtual const std::string& GetUserPresentingWindow(
      aura::Window* window) const OVERRIDE;
  virtual void AddUser(content::BrowserContext* context) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiUserWindowManagerStub);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_STUB_H_
