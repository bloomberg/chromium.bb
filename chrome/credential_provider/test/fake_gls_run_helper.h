// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_TEST_FAKE_GLS_RUN_HELPER_H_
#define CHROME_CREDENTIAL_PROVIDER_TEST_FAKE_GLS_RUN_HELPER_H_

struct ICredentialProviderCredential;

#include "base/test/test_reg_util_win.h"
#include "chrome/credential_provider/test/com_fakes.h"
#include "chrome/credential_provider/test/gcp_fakes.h"

namespace credential_provider {

namespace testing {
extern const char kDefaultEmail[];
extern const wchar_t kDefaultUsername[];
}  // namespace testing

// Helper class used to run and wait for a fake GLS process to validate the
// functionality of a GCPW credential.
class FakeGlsRunHelper {
 public:
  FakeGlsRunHelper();
  ~FakeGlsRunHelper();

  void SetUp();

  HRESULT StartLogonProcess(ICredentialProviderCredential* cred, bool succeeds);
  HRESULT WaitForLogonProcess(ICredentialProviderCredential* cred);
  HRESULT StartLogonProcessAndWait(ICredentialProviderCredential* cred);

  FakeOSUserManager* fake_os_user_manager() { return &fake_os_user_manager_; }
  static HRESULT GetFakeGlsCommandline(
      const std::string& gls_email,
      const std::string& gaia_id_override,
      const base::string16& start_gls_event_name,
      base::CommandLine* command_line);

 private:
  FakeOSProcessManager fake_os_process_manager_;
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
  registry_util::RegistryOverrideManager registry_override_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_FAKE_GLS_RUN_HELPER_H_
