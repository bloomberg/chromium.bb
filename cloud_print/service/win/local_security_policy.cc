// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/local_security_policy.h"

#include <atlsecurity.h>
#include <ntsecapi.h>
#include <windows.h>

#include "base/logging.h"
#include "base/strings/string_util.h"

const wchar_t kSeServiceLogonRight[] = L"SeServiceLogonRight";

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

namespace {

template<class T>
class ScopedLsaMemory {
 public:
  ScopedLsaMemory() : lsa_memory_(NULL) {
  }

  ~ScopedLsaMemory() {
    Close();
  }

  void Close() {
    if (lsa_memory_) {
      LsaFreeMemory(lsa_memory_);
      lsa_memory_ = NULL;
    }
  }

  T* Get() const {
    return lsa_memory_;
  }

  T** Receive() {
    Close();
    return &lsa_memory_;
  }

 private:
  T* lsa_memory_;
  DISALLOW_COPY_AND_ASSIGN(ScopedLsaMemory);
};

}  // namespace

LocalSecurityPolicy::LocalSecurityPolicy() : policy_(NULL) {
}

LocalSecurityPolicy::~LocalSecurityPolicy() {
  Close();
}

void LocalSecurityPolicy::Close() {
  if (policy_) {
    LsaClose(policy_);
    policy_ = NULL;
  }
}

bool LocalSecurityPolicy::Open() {
  DCHECK(!policy_);
  Close();
  LSA_OBJECT_ATTRIBUTES attributes = {0};
  return STATUS_SUCCESS ==
      ::LsaOpenPolicy(NULL, &attributes,
                      POLICY_CREATE_ACCOUNT | POLICY_LOOKUP_NAMES,
                      &policy_);
}

bool LocalSecurityPolicy::IsPrivilegeSet(
    const base::string16& username,
    const base::string16& privilage) const {
  DCHECK(policy_);
  ATL::CSid user_sid;
  if (!user_sid.LoadAccount(username.c_str())) {
    LOG(ERROR) << "Unable to load Sid for" << username;
    return false;
  }
  ScopedLsaMemory<LSA_UNICODE_STRING> rights;
  ULONG count = 0;
  NTSTATUS status = ::LsaEnumerateAccountRights(
      policy_, const_cast<SID*>(user_sid.GetPSID()), rights.Receive(), &count);
  if (STATUS_SUCCESS != status || !rights.Get())
    return false;
  for (size_t i = 0; i < count; ++i) {
    if (privilage == rights.Get()[i].Buffer)
      return true;
  }
  return false;
}

bool LocalSecurityPolicy::SetPrivilege(const base::string16& username,
                                       const base::string16& privilage) {
  DCHECK(policy_);
  ATL::CSid user_sid;
  if (!user_sid.LoadAccount(username.c_str())) {
    LOG(ERROR) << "Unable to load Sid for" << username;
    return false;
  }
  LSA_UNICODE_STRING privilege_string;
  base::string16 privilage_copy(privilage);
  privilege_string.Buffer = &privilage_copy[0];
  privilege_string.Length = wcslen(privilege_string.Buffer) *
                            sizeof(privilege_string.Buffer[0]);
  privilege_string.MaximumLength = privilege_string.Length +
                                   sizeof(privilege_string.Buffer[0]);
  return STATUS_SUCCESS ==
      ::LsaAddAccountRights(policy_, const_cast<SID*>(user_sid.GetPSID()),
                            &privilege_string, 1);
}

