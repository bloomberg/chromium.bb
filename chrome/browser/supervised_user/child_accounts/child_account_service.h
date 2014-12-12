// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/child_accounts/family_info_fetcher.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_service_flag_fetcher.h"
#include "components/signin/core/browser/signin_manager_base.h"

namespace base {
class FilePath;
}

class Profile;

// This class handles detection of child accounts (on sign-in as well as on
// browser restart), and triggers the appropriate behavior (e.g. enable the
// supervised user experience, fetch information about the parent(s)).
class ChildAccountService : public KeyedService,
                            public FamilyInfoFetcher::Consumer,
                            public SigninManagerBase::Observer,
                            public SupervisedUserService::Delegate {
 public:
  virtual ~ChildAccountService();

  void Init();

  // Sets whether the signed-in account is a child account.
  // Public so it can be called on platforms where child account detection
  // happens outside of this class (like Android).
  void SetIsChildAccount(bool is_child_account);

  // KeyedService:
  virtual void Shutdown() override;

 private:
  friend class ChildAccountServiceFactory;
  // Use |ChildAccountServiceFactory::GetForProfile(...)| to get an instance of
  // this service.
  explicit ChildAccountService(Profile* profile);

  // SupervisedUserService::Delegate implementation.
  virtual bool SetActive(bool active) override;
  virtual base::FilePath GetBlacklistPath() const override;
  virtual GURL GetBlacklistURL() const override;
  virtual std::string GetSafeSitesCx() const override;

  // SigninManagerBase::Observer implementation.
  virtual void GoogleSigninSucceeded(const std::string& account_id,
                                     const std::string& username,
                                     const std::string& password) override;
  virtual void GoogleSignedOut(const std::string& account_id,
                               const std::string& username) override;

  // FamilyInfoFetcher::Consumer implementation.
  virtual void OnGetFamilyMembersSuccess(
      const std::vector<FamilyInfoFetcher::FamilyMember>& members) override;
  virtual void OnFailure(FamilyInfoFetcher::ErrorCode error) override;

  void StartFetchingServiceFlags(const std::string& account_id);
  void CancelFetchingServiceFlags();
  void OnFlagsFetched(AccountServiceFlagFetcher::ResultCode,
                      const std::vector<std::string>& flags);

  void PropagateChildStatusToUser(bool is_child);

  void SetFirstCustodianPrefs(const FamilyInfoFetcher::FamilyMember& custodian);
  void SetSecondCustodianPrefs(
      const FamilyInfoFetcher::FamilyMember& custodian);
  void ClearFirstCustodianPrefs();
  void ClearSecondCustodianPrefs();

  void EnableExperimentalFiltering();

  // Owns us via the KeyedService mechanism.
  Profile* profile_;

  bool active_;

  // The user for which we are currently trying to fetch the child account flag.
  // Empty when we are not currently fetching.
  std::string account_id_;

  scoped_ptr<AccountServiceFlagFetcher> flag_fetcher_;

  scoped_ptr<FamilyInfoFetcher> family_fetcher_;

  base::WeakPtrFactory<ChildAccountService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildAccountService);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_H_
