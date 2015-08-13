// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/sync/browser/sync_store_result_filter.h"

#include "base/command_line.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using testing::_;
using testing::Return;
using testing::ReturnRef;

namespace password_manager {

namespace {

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_CONST_METHOD2(IsSyncAccountCredential,
                     bool(const std::string&, const std::string&));
  MOCK_CONST_METHOD0(GetLastCommittedEntryURL, const GURL&());
};

bool IsFormFiltered(const StoreResultFilter& filter, const PasswordForm& form) {
  ScopedVector<PasswordForm> vector;
  vector.push_back(new PasswordForm(form));
  vector = filter.FilterResults(vector.Pass());
  return vector.empty();
}

}  // namespace

TEST(StoreResultFilterTest, ShouldFilterAutofillResult_Reauth) {
  // Disallow only reauth requests.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(
      switches::kDisallowAutofillSyncCredentialForReauth);
  PasswordForm form;
  MockPasswordManagerClient client;
  SyncStoreResultFilter filter(&client);

  EXPECT_CALL(client, IsSyncAccountCredential(_, _))
      .WillRepeatedly(Return(false));
  GURL rart_countinue_url(
      "https://accounts.google.com/login?rart=123&continue=blah");
  EXPECT_CALL(client, GetLastCommittedEntryURL())
      .WillRepeatedly(ReturnRef(rart_countinue_url));
  EXPECT_FALSE(IsFormFiltered(filter, form));

  EXPECT_CALL(client, IsSyncAccountCredential(_, _))
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(IsFormFiltered(filter, form));

  // This counts as a reauth url, though a valid URL should have a value for
  // "rart"
  GURL rart_url("https://accounts.google.com/addlogin?rart");
  EXPECT_CALL(client, GetLastCommittedEntryURL()).WillOnce(ReturnRef(rart_url));
  EXPECT_TRUE(IsFormFiltered(filter, form));

  GURL param_url("https://accounts.google.com/login?param=123");
  EXPECT_CALL(client, GetLastCommittedEntryURL())
      .WillOnce(ReturnRef(param_url));
  EXPECT_FALSE(IsFormFiltered(filter, form));

  GURL rart_value_url("https://site.com/login?rart=678");
  EXPECT_CALL(client, GetLastCommittedEntryURL())
      .WillOnce(ReturnRef(rart_value_url));
  EXPECT_FALSE(IsFormFiltered(filter, form));
}

TEST(StoreResultFilterTest, ShouldFilterAutofillResult) {
  // Normally, no credentials should be filtered, even if they are the sync
  // credential.
  PasswordForm form;
  MockPasswordManagerClient client;
  SyncStoreResultFilter filter(&client);

  EXPECT_CALL(client, IsSyncAccountCredential(_, _))
      .WillRepeatedly(Return(true));
  GURL login_url("https://accounts.google.com/Login");
  EXPECT_CALL(client, GetLastCommittedEntryURL())
      .WillRepeatedly(ReturnRef(login_url));
  EXPECT_FALSE(IsFormFiltered(filter, form));

  // Adding disallow switch should cause sync credential to be filtered.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kDisallowAutofillSyncCredential);
  SyncStoreResultFilter filter_disallow_sync_cred(&client);
  EXPECT_TRUE(IsFormFiltered(filter_disallow_sync_cred, form));
}

}  // namespace password_manager
