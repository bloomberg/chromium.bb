// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_fetcher_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/browser/stub_credentials_filter.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using base::StringPiece;
using testing::_;
using testing::IsEmpty;
using testing::Pointee;
using testing::Return;
using testing::UnorderedElementsAre;
using testing::WithArg;

namespace password_manager {

namespace {

class MockConsumer : public FormFetcher::Consumer {
 public:
  MOCK_METHOD2(ProcessMatches,
               void(const std::vector<const PasswordForm*>& non_federated,
                    size_t filtered_count));
};

class NameFilter : public StubCredentialsFilter {
 public:
  // This class filters out all credentials which have |name| as
  // |username_value|.
  explicit NameFilter(StringPiece name) : name_(ASCIIToUTF16(name)) {}

  ~NameFilter() override = default;

  std::vector<std::unique_ptr<PasswordForm>> FilterResults(
      std::vector<std::unique_ptr<PasswordForm>> results) const override {
    results.erase(
        std::remove_if(results.begin(), results.end(),
                       [this](const std::unique_ptr<PasswordForm>& form) {
                         return !ShouldSave(*form);
                       }),
        results.end());
    return results;
  }

  bool ShouldSave(const PasswordForm& form) const override {
    return form.username_value != name_;
  }

 private:
  const base::string16 name_;  // |username_value| to filter

  DISALLOW_COPY_AND_ASSIGN(NameFilter);
};

class FakePasswordManagerClient : public StubPasswordManagerClient {
 public:
  FakePasswordManagerClient() = default;
  ~FakePasswordManagerClient() override = default;

  void set_filter(std::unique_ptr<CredentialsFilter> filter) {
    filter_ = std::move(filter);
  }

  void set_store(PasswordStore* store) { store_ = store; }

 private:
  const CredentialsFilter* GetStoreResultFilter() const override {
    return filter_ ? filter_.get()
                   : StubPasswordManagerClient::GetStoreResultFilter();
  }

  PasswordStore* GetPasswordStore() const override { return store_; }

  std::unique_ptr<CredentialsFilter> filter_;
  PasswordStore* store_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakePasswordManagerClient);
};

// Creates a dummy non-federated form with some basic arbitrary values.
PasswordForm CreateNonFederated() {
  PasswordForm form;
  form.origin = GURL("https://example.in");
  form.signon_realm = form.origin.spec();
  form.action = GURL("https://login.example.org");
  form.username_value = ASCIIToUTF16("user");
  form.password_value = ASCIIToUTF16("password");
  return form;
}

// Creates a dummy non-federated HTTP form with some basic arbitrary values.
PasswordForm CreateHTTPNonFederated() {
  PasswordForm form;
  form.origin = GURL("http://example.in");
  form.signon_realm = form.origin.spec();
  form.action = GURL("http://login.example.org");
  form.username_value = ASCIIToUTF16("user");
  form.password_value = ASCIIToUTF16("password");
  return form;
}

// Creates a dummy federated form with some basic arbitrary values.
PasswordForm CreateFederated() {
  PasswordForm form = CreateNonFederated();
  form.password_value.clear();
  form.federation_origin = url::Origin(GURL("https://accounts.google.com/"));
  return form;
}

// Creates an Android federated credential.
PasswordForm CreateAndroidFederated() {
  PasswordForm form = CreateFederated();
  form.signon_realm = "android://hash@com.example.android/";
  form.origin = GURL(form.signon_realm);
  form.action = GURL();
  form.is_affiliation_based_match = true;
  return form;
}

// Small helper that wraps passed in forms in unique ptrs.
std::vector<std::unique_ptr<PasswordForm>> MakeResults(
    const std::vector<PasswordForm>& forms) {
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.reserve(forms.size());
  for (const auto& form : forms)
    results.push_back(base::MakeUnique<PasswordForm>(form));
  return results;
}

class MockFormFetcherImpl : public FormFetcherImpl {
 public:
  // Inherit constructors.
  using FormFetcherImpl::FormFetcherImpl;

  // Google Mock is currently unable to mock |ProcessMigratedForms| due to the
  // presence of move-only types. In order to ensure it is called, a dummy is
  // added which can be passed to |EXPECT_CALL|.
  void ProcessMigratedForms(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override {
    FormFetcherImpl::ProcessMigratedForms(std::move(results));
    ProcessMigratedFormsDummy();
  }

  MOCK_METHOD0(ProcessMigratedFormsDummy, void());
};

ACTION_P(GetAndAssignWeakPtr, ptr) {
  *ptr = arg0->GetWeakPtr();
}

}  // namespace

class FormFetcherImplTest : public testing::Test {
 public:
  FormFetcherImplTest()
      : form_digest_(PasswordForm::SCHEME_HTML,
                     "http://accounts.google.com",
                     GURL("http://accounts.google.com/a/LoginAuth")) {
    mock_store_ = new MockPasswordStore();
    client_.set_store(mock_store_.get());

    form_fetcher_ = base::MakeUnique<FormFetcherImpl>(
        form_digest_, &client_, /* should_migrate_http_passwords */ false);
  }

  ~FormFetcherImplTest() override { mock_store_->ShutdownOnUIThread(); }

 protected:
  // A wrapper around form_fetcher_.Fetch(), adding the call expectations.
  void Fetch() {
#if !defined(OS_IOS) && !defined(OS_ANDROID)
    EXPECT_CALL(*mock_store_, GetSiteStatsImpl(_))
        .WillOnce(Return(std::vector<InteractionsStats>()));
#endif
    EXPECT_CALL(*mock_store_, GetLogins(form_digest_, form_fetcher_.get()));
    form_fetcher_->Fetch();
    base::RunLoop().RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(mock_store_.get());
  }

  base::MessageLoop message_loop_;  // Used by mock_store_.
  PasswordStore::FormDigest form_digest_;
  std::unique_ptr<FormFetcherImpl> form_fetcher_;
  MockConsumer consumer_;
  scoped_refptr<MockPasswordStore> mock_store_;
  FakePasswordManagerClient client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormFetcherImplTest);
};

// Check that the absence of PasswordStore results is handled correctly.
TEST_F(FormFetcherImplTest, NoStoreResults) {
  Fetch();
  EXPECT_CALL(consumer_, ProcessMatches(_, _)).Times(0);
  form_fetcher_->AddConsumer(&consumer_);
  EXPECT_EQ(FormFetcher::State::WAITING, form_fetcher_->GetState());
}

// Check that empty PasswordStore results are handled correctly.
TEST_F(FormFetcherImplTest, Empty) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  EXPECT_CALL(consumer_, ProcessMatches(IsEmpty(), 0u));
  form_fetcher_->OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>>());
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
}

// Check that non-federated PasswordStore results are handled correctly.
TEST_F(FormFetcherImplTest, NonFederated) {
  Fetch();
  PasswordForm non_federated = CreateNonFederated();
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(base::MakeUnique<PasswordForm>(non_federated));
  EXPECT_CALL(consumer_,
              ProcessMatches(UnorderedElementsAre(Pointee(non_federated)), 0u));
  form_fetcher_->OnGetPasswordStoreResults(std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());
}

// Check that federated PasswordStore results are handled correctly.
TEST_F(FormFetcherImplTest, Federated) {
  Fetch();
  PasswordForm federated = CreateFederated();
  PasswordForm android_federated = CreateAndroidFederated();
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(base::MakeUnique<PasswordForm>(federated));
  results.push_back(base::MakeUnique<PasswordForm>(android_federated));
  EXPECT_CALL(consumer_, ProcessMatches(IsEmpty(), 0u));
  form_fetcher_->OnGetPasswordStoreResults(std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(
      form_fetcher_->GetFederatedMatches(),
      UnorderedElementsAre(Pointee(federated), Pointee(android_federated)));
}

// Check that mixed PasswordStore results are handled correctly.
TEST_F(FormFetcherImplTest, Mixed) {
  Fetch();
  PasswordForm federated1 = CreateFederated();
  federated1.username_value = ASCIIToUTF16("user");
  PasswordForm federated2 = CreateFederated();
  federated2.username_value = ASCIIToUTF16("user_B");
  PasswordForm federated3 = CreateAndroidFederated();
  federated3.username_value = ASCIIToUTF16("user_B");
  PasswordForm non_federated1 = CreateNonFederated();
  non_federated1.username_value = ASCIIToUTF16("user");
  PasswordForm non_federated2 = CreateNonFederated();
  non_federated2.username_value = ASCIIToUTF16("user_C");
  PasswordForm non_federated3 = CreateNonFederated();
  non_federated3.username_value = ASCIIToUTF16("user_D");

  form_fetcher_->AddConsumer(&consumer_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(base::MakeUnique<PasswordForm>(federated1));
  results.push_back(base::MakeUnique<PasswordForm>(federated2));
  results.push_back(base::MakeUnique<PasswordForm>(federated3));
  results.push_back(base::MakeUnique<PasswordForm>(non_federated1));
  results.push_back(base::MakeUnique<PasswordForm>(non_federated2));
  results.push_back(base::MakeUnique<PasswordForm>(non_federated3));
  EXPECT_CALL(consumer_,
              ProcessMatches(UnorderedElementsAre(Pointee(non_federated1),
                                                  Pointee(non_federated2),
                                                  Pointee(non_federated3)),
                             0u));
  form_fetcher_->OnGetPasswordStoreResults(std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(Pointee(federated1), Pointee(federated2),
                                   Pointee(federated3)));
}

// Check that PasswordStore results are filtered correctly.
TEST_F(FormFetcherImplTest, Filtered) {
  Fetch();
  PasswordForm federated = CreateFederated();
  federated.username_value = ASCIIToUTF16("user");
  PasswordForm non_federated1 = CreateNonFederated();
  non_federated1.username_value = ASCIIToUTF16("user");
  PasswordForm non_federated2 = CreateNonFederated();
  non_federated2.username_value = ASCIIToUTF16("user_C");

  // Set up a filter to remove all credentials with the username "user".
  client_.set_filter(base::MakeUnique<NameFilter>("user"));

  form_fetcher_->AddConsumer(&consumer_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(base::MakeUnique<PasswordForm>(federated));
  results.push_back(base::MakeUnique<PasswordForm>(non_federated1));
  results.push_back(base::MakeUnique<PasswordForm>(non_federated2));
  // Non-federated results should have been filtered: no "user" here.
  constexpr size_t kNumFiltered = 1u;
  EXPECT_CALL(consumer_,
              ProcessMatches(UnorderedElementsAre(Pointee(non_federated2)),
                             kNumFiltered));
  form_fetcher_->OnGetPasswordStoreResults(std::move(results));
  EXPECT_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  // However, federated results should not be filtered.
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(Pointee(federated)));
}

// Check that stats from PasswordStore are handled correctly.
TEST_F(FormFetcherImplTest, Stats) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  std::vector<InteractionsStats> stats(1);
  form_fetcher_->OnGetSiteStatistics(std::move(stats));
  EXPECT_EQ(1u, form_fetcher_->GetInteractionsStats().size());
}

// Test that multiple calls of Fetch() are handled gracefully, and that they
// always result in passing the most up-to-date information to the consumers.
TEST_F(FormFetcherImplTest, Update_Reentrance) {
  Fetch();
  form_fetcher_->AddConsumer(&consumer_);
  // The fetcher is currently waiting for a store response, after it fired a
  // GetLogins request during the Fetch() above. The second and third Fetch
  // (below) won't cause a GetLogins right now, but will ensure that a second
  // GetLogins will be called later.
  form_fetcher_->Fetch();
  form_fetcher_->Fetch();

  // First response from the store, should be ignored.
  PasswordForm form_a = CreateNonFederated();
  form_a.username_value = ASCIIToUTF16("a@gmail.com");
  std::vector<std::unique_ptr<PasswordForm>> old_results;
  old_results.push_back(base::MakeUnique<PasswordForm>(form_a));
  // Because of the pending updates, the old PasswordStore results are not
  // forwarded to the consumers.
  EXPECT_CALL(consumer_, ProcessMatches(_, _)).Times(0);
  // Delivering the first results will trigger the new GetLogins call, because
  // of the Fetch() above.
  EXPECT_CALL(*mock_store_, GetLogins(form_digest_, form_fetcher_.get()));
  form_fetcher_->OnGetPasswordStoreResults(std::move(old_results));

  // Second response from the store should not be ignored.
  PasswordForm form_b = CreateNonFederated();
  form_b.username_value = ASCIIToUTF16("b@gmail.com");

  PasswordForm form_c = CreateNonFederated();
  form_c.username_value = ASCIIToUTF16("c@gmail.com");

  EXPECT_CALL(consumer_,
              ProcessMatches(
                  UnorderedElementsAre(Pointee(form_b), Pointee(form_c)), 0u));
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.push_back(base::MakeUnique<PasswordForm>(form_b));
  results.push_back(base::MakeUnique<PasswordForm>(form_c));
  form_fetcher_->OnGetPasswordStoreResults(std::move(results));
}

#if !defined(OS_IOS) && !defined(OS_ANDROID)
TEST_F(FormFetcherImplTest, FetchStatistics) {
  InteractionsStats stats;
  stats.origin_domain = form_digest_.origin.GetOrigin();
  stats.username_value = ASCIIToUTF16("some username");
  stats.dismissal_count = 5;
  std::vector<InteractionsStats> db_stats = {stats};
  EXPECT_CALL(*mock_store_, GetLogins(form_digest_, form_fetcher_.get()));
  EXPECT_CALL(*mock_store_, GetSiteStatsImpl(stats.origin_domain))
      .WillOnce(Return(db_stats));
  form_fetcher_->Fetch();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(form_fetcher_->GetInteractionsStats(),
              UnorderedElementsAre(stats));
}
#else
TEST_F(FormFetcherImplTest, DontFetchStatistics) {
  EXPECT_CALL(*mock_store_, GetLogins(form_digest_, form_fetcher_.get()));
  EXPECT_CALL(*mock_store_, GetSiteStatsImpl(_)).Times(0);
  form_fetcher_->Fetch();
  base::RunLoop().RunUntilIdle();
}
#endif

// Test that ensures HTTP passwords are not migrated on HTTP sites.
TEST_F(FormFetcherImplTest, DoNotTryToMigrateHTTPPasswordsOnHTTPSites) {
  GURL::Replacements http_rep;
  http_rep.SetSchemeStr(url::kHttpScheme);
  const GURL http_origin = form_digest_.origin.ReplaceComponents(http_rep);
  form_digest_ = PasswordStore::FormDigest(
      PasswordForm::SCHEME_HTML, http_origin.GetOrigin().spec(), http_origin);

  // A new form fetcher is created to be able to set the form digest and
  // migration flag.
  form_fetcher_ = base::MakeUnique<MockFormFetcherImpl>(
      form_digest_, &client_, /* should_migrate_http_passwords */ true);
  EXPECT_CALL(consumer_, ProcessMatches(IsEmpty(), 0u));
  form_fetcher_->AddConsumer(&consumer_);

  std::vector<PasswordForm> empty_forms;
  const PasswordForm http_form = CreateHTTPNonFederated();
  const PasswordForm federated_form = CreateFederated();

  Fetch();
  EXPECT_CALL(*mock_store_, GetLogins(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(consumer_, ProcessMatches(IsEmpty(), 0u));
  form_fetcher_->OnGetPasswordStoreResults(MakeResults(empty_forms));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());

  Fetch();
  EXPECT_CALL(consumer_,
              ProcessMatches(UnorderedElementsAre(Pointee(http_form)), 0u));
  form_fetcher_->OnGetPasswordStoreResults(MakeResults({http_form}));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());

  Fetch();
  EXPECT_CALL(consumer_,
              ProcessMatches(UnorderedElementsAre(Pointee(http_form)), 0u));
  form_fetcher_->OnGetPasswordStoreResults(
      MakeResults({http_form, federated_form}));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(Pointee(federated_form)));
}

// Test that ensures HTTP passwords are only migrated on HTTPS sites when no
// HTTPS credentials are available.
TEST_F(FormFetcherImplTest, TryToMigrateHTTPPasswordsOnHTTPSSites) {
  GURL::Replacements https_rep;
  https_rep.SetSchemeStr(url::kHttpsScheme);
  const GURL https_origin = form_digest_.origin.ReplaceComponents(https_rep);
  form_digest_ = PasswordStore::FormDigest(
      PasswordForm::SCHEME_HTML, https_origin.GetOrigin().spec(), https_origin);

  // A new form fetcher is created to be able to set the form digest and
  // migration flag.
  form_fetcher_ = base::MakeUnique<MockFormFetcherImpl>(
      form_digest_, &client_, /* should_migrate_http_passwords */ true);
  EXPECT_CALL(consumer_, ProcessMatches(IsEmpty(), 0u));
  form_fetcher_->AddConsumer(&consumer_);

  PasswordForm https_form = CreateNonFederated();

  // Create HTTP form for the same orgin (except scheme), which will be passed
  // to the migrator.
  GURL::Replacements http_rep;
  http_rep.SetSchemeStr(url::kHttpScheme);
  PasswordForm http_form = https_form;
  http_form.origin = https_form.origin.ReplaceComponents(http_rep);
  http_form.signon_realm = http_form.origin.GetOrigin().spec();

  std::vector<PasswordForm> empty_forms;

  // Tests that there is only an attempt to migrate credentials on HTTPS origins
  // when no other credentials are available.
  const GURL form_digest_http_origin =
      form_digest_.origin.ReplaceComponents(http_rep);
  PasswordStore::FormDigest http_form_digest(
      PasswordForm::SCHEME_HTML, form_digest_http_origin.GetOrigin().spec(),
      form_digest_http_origin);
  Fetch();
  base::WeakPtr<PasswordStoreConsumer> migrator_ptr;
  EXPECT_CALL(*mock_store_, GetLogins(http_form_digest, _))
      .WillOnce(WithArg<1>(GetAndAssignWeakPtr(&migrator_ptr)));
  form_fetcher_->OnGetPasswordStoreResults(MakeResults(empty_forms));
  ASSERT_TRUE(migrator_ptr);

  // Now perform the actual migration.
  EXPECT_CALL(*mock_store_, AddLogin(https_form));
  EXPECT_CALL(*static_cast<MockFormFetcherImpl*>(form_fetcher_.get()),
              ProcessMigratedFormsDummy());
  EXPECT_CALL(consumer_,
              ProcessMatches(UnorderedElementsAre(Pointee(https_form)), 0u));
  static_cast<HttpPasswordMigrator*>(migrator_ptr.get())
      ->OnGetPasswordStoreResults(MakeResults({http_form}));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());

  // No migration should happen when results are present.
  Fetch();
  EXPECT_CALL(*mock_store_, GetLogins(_, _)).Times(0);
  EXPECT_CALL(*mock_store_, AddLogin(_)).Times(0);
  EXPECT_CALL(consumer_,
              ProcessMatches(UnorderedElementsAre(Pointee(https_form)), 0u));
  form_fetcher_->OnGetPasswordStoreResults(MakeResults({https_form}));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(), IsEmpty());

  const PasswordForm federated_form = CreateFederated();
  Fetch();
  EXPECT_CALL(consumer_,
              ProcessMatches(UnorderedElementsAre(Pointee(https_form)), 0u));
  form_fetcher_->OnGetPasswordStoreResults(
      MakeResults({https_form, federated_form}));
  EXPECT_THAT(form_fetcher_->GetFederatedMatches(),
              UnorderedElementsAre(Pointee(federated_form)));
}

}  // namespace password_manager
