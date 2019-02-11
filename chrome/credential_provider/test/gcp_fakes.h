// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_TEST_GCP_FAKES_H_
#define CHROME_CREDENTIAL_PROVIDER_TEST_GCP_FAKES_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"
#include "chrome/credential_provider/gaiacp/os_process_manager.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"
#include "chrome/credential_provider/gaiacp/scoped_user_profile.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {

///////////////////////////////////////////////////////////////////////////////

class FakeOSProcessManager : public OSProcessManager {
 public:
  FakeOSProcessManager();
  ~FakeOSProcessManager() override;

  // OSProcessManager
  HRESULT GetTokenLogonSID(const base::win::ScopedHandle& token,
                           PSID* sid) override;
  HRESULT SetupPermissionsForLogonSid(PSID sid) override;
  HRESULT CreateProcessWithToken(
      const base::win::ScopedHandle& logon_token,
      const base::CommandLine& command_line,
      _STARTUPINFOW* startupinfo,
      base::win::ScopedProcessInformation* procinfo) override;

 private:
  OSProcessManager* original_manager_;
  DWORD next_rid_ = 0;
};

///////////////////////////////////////////////////////////////////////////////

class FakeOSUserManager : public OSUserManager {
 public:
  FakeOSUserManager();
  ~FakeOSUserManager() override;

  // OSUserManager
  HRESULT GenerateRandomPassword(wchar_t* password, int length) override;
  HRESULT AddUser(const wchar_t* username,
                  const wchar_t* password,
                  const wchar_t* fullname,
                  const wchar_t* comment,
                  bool add_to_users_group,
                  BSTR* sid,
                  DWORD* error) override;
  HRESULT ChangeUserPassword(const wchar_t* username,
                             const wchar_t* password,
                             const wchar_t* old_password) override;
  HRESULT IsWindowsPasswordValid(const wchar_t* username,
                                 const wchar_t* password) override;

  HRESULT CreateLogonToken(const wchar_t* username,
                           const wchar_t* password,
                           bool interactive,
                           base::win::ScopedHandle* token) override;
  HRESULT GetUserSID(const wchar_t* username, PSID* sid) override;
  HRESULT FindUserBySID(const wchar_t* sid,
                        wchar_t* username,
                        DWORD length) override;
  HRESULT RemoveUser(const wchar_t* username, const wchar_t* password) override;

  struct UserInfo {
    UserInfo(const wchar_t* password,
             const wchar_t* fullname,
             const wchar_t* comment,
             const wchar_t* sid);
    UserInfo();
    UserInfo(const UserInfo& other);
    ~UserInfo();

    bool operator==(const UserInfo& other) const;

    base::string16 password;
    base::string16 fullname;
    base::string16 comment;
    base::string16 sid;
  };
  const UserInfo GetUserInfo(const wchar_t* username);

  // Creates a new unique sid.  Free returned sid with FreeSid().
  HRESULT CreateNewSID(PSID* sid);

  // Creates a fake user with the given |username|, |password|, |fullname|,
  // |comment|. If |gaia_id| is non-empty, also associates the user with
  // the given gaia id. If |email| is non-empty, sets the email to use for
  // reauth to be this one.
  // |sid| is allocated and filled with the SID of the new user.
  HRESULT CreateTestOSUser(const base::string16& username,
                           const base::string16& password,
                           const base::string16& fullname,
                           const base::string16& comment,
                           const base::string16& gaia_id,
                           const base::string16& email,
                           BSTR* sid);

  size_t GetUserCount() const { return username_to_info_.size(); }

 private:
  OSUserManager* original_manager_;
  DWORD next_rid_ = 0;
  std::map<base::string16, UserInfo> username_to_info_;
};

///////////////////////////////////////////////////////////////////////////////

class FakeScopedLsaPolicyFactory {
 public:
  FakeScopedLsaPolicyFactory();
  virtual ~FakeScopedLsaPolicyFactory();

  ScopedLsaPolicy::CreatorCallback GetCreatorCallback();

  // PrivateDataMap is a string-to-string key/value store that maps private
  // names to their corresponding data strings.  The term "private" here is
  // used to reflect the name of the underlying OS calls.  This data is meant
  // to be shared by all ScopedLsaPolicy instances created by this factory.
  using PrivateDataMap = std::map<base::string16, base::string16>;
  PrivateDataMap& private_data() { return private_data_; }

 private:
  std::unique_ptr<ScopedLsaPolicy> Create(ACCESS_MASK mask);

  ScopedLsaPolicy::CreatorCallback original_creator_;
  PrivateDataMap private_data_;
};

class FakeScopedLsaPolicy : public ScopedLsaPolicy {
 public:
  ~FakeScopedLsaPolicy() override;

  // ScopedLsaPolicy
  HRESULT StorePrivateData(const wchar_t* key, const wchar_t* value) override;
  HRESULT RemovePrivateData(const wchar_t* key) override;
  HRESULT RetrievePrivateData(const wchar_t* key,
                              wchar_t* value,
                              size_t length) override;
  HRESULT AddAccountRights(PSID sid, const wchar_t* right) override;
  HRESULT RemoveAccount(PSID sid) override;

 private:
  friend class FakeScopedLsaPolicyFactory;

  explicit FakeScopedLsaPolicy(FakeScopedLsaPolicyFactory* factory);

  FakeScopedLsaPolicyFactory::PrivateDataMap& private_data() {
    return factory_->private_data();
  }

  FakeScopedLsaPolicyFactory* factory_;
};

///////////////////////////////////////////////////////////////////////////////

// A scoped FakeScopedUserProfile factory.  Installs itself when constructed
// and removes itself when deleted.
class FakeScopedUserProfileFactory {
 public:
  FakeScopedUserProfileFactory();
  virtual ~FakeScopedUserProfileFactory();

 private:
  std::unique_ptr<ScopedUserProfile> Create(const base::string16& sid,
                                            const base::string16& username,
                                            const base::string16& password);

  ScopedUserProfile::CreatorCallback original_creator_;
};

class FakeScopedUserProfile : public ScopedUserProfile {
 public:
  HRESULT SaveAccountInfo(const base::DictionaryValue& properties) override;

 private:
  friend class FakeScopedUserProfileFactory;

  FakeScopedUserProfile(const base::string16& sid,
                        const base::string16& username,
                        const base::string16& password);
  ~FakeScopedUserProfile() override;

  bool is_valid_ = false;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_GCP_FAKES_H_
