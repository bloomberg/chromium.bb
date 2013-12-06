// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_SERVICE_WIN_LOCAL_SECURITY_POLICY_H_
#define CLOUD_PRINT_SERVICE_WIN_LOCAL_SECURITY_POLICY_H_

#include <wtypes.h>  // Has to be before ntsecapi.h
#include <ntsecapi.h>

#include "base/basictypes.h"
#include "base/strings/string16.h"

extern const wchar_t kSeServiceLogonRight[];

class LocalSecurityPolicy {
 public:
  LocalSecurityPolicy();
  ~LocalSecurityPolicy();

  bool Open();
  void Close();

  bool IsPrivilegeSet(const base::string16& username,
                      const base::string16& privilage) const;
  bool SetPrivilege(const base::string16& username,
                    const base::string16& privilage);

 private:
  LSA_HANDLE policy_;

  DISALLOW_COPY_AND_ASSIGN(LocalSecurityPolicy);
};

#endif  // CLOUD_PRINT_SERVICE_WIN_LOCAL_SECURITY_POLICY_H_
