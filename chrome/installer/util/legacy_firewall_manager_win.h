// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_LEGACY_FIREWALL_MANAGER_WIN_H_
#define CHROME_INSTALLER_UTIL_LEGACY_FIREWALL_MANAGER_WIN_H_

#include <windows.h>
#include <netfw.h>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/win/scoped_comptr.h"

namespace installer {

// Manages firewall rules using Windows Firewall API. The API is
// available on Windows XP with SP2 and later. Applications should use
// |AdvancedFirewallManager| instead of this class on Windows Vista and later.
// Most methods need elevation.
class LegacyFirewallManager {
 public:
  LegacyFirewallManager();
  ~LegacyFirewallManager();

  // Initializes object to manage application win name |app_name| and path
  // |app_path|.
  bool Init(const base::string16& app_name, const base::FilePath& app_path);

  // Returns true if firewall is enabled.
  bool IsFirewallEnabled();

  // Returns true if function can read rule for the current app. Sets |value|
  // true, if rule allows incoming connections, or false otherwise.
  bool GetAllowIncomingConnection(bool* value);

  // Allows or blocks all incoming connection for current app. Needs elevation.
  bool SetAllowIncomingConnection(bool allow);

  // Deletes rule for current app. Needs elevation.
  void DeleteRule();

 private:
  // Returns the authorized applications collection for the local firewall
  // policy's current profile or an empty pointer in case of error.
  base::win::ScopedComPtr<INetFwAuthorizedApplications>
      GetAuthorizedApplications();

  // Creates rule for the current application. If |allow| is true, incoming
  // connections are allowed, blocked otherwise.
  base::win::ScopedComPtr<INetFwAuthorizedApplication>
      CreateChromeAuthorization(bool allow);

  base::string16 app_name_;
  base::FilePath app_path_;
  base::win::ScopedComPtr<INetFwProfile> current_profile_;

  DISALLOW_COPY_AND_ASSIGN(LegacyFirewallManager);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_LEGACY_FIREWALL_MANAGER_WIN_H_
