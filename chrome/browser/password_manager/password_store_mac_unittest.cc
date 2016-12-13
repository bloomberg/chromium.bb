// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_mac.h"

#include <stddef.h>

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/password_manager/password_store_mac_internal.h"
#include "chrome/common/chrome_paths.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_origin_unittest.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "crypto/mock_apple_keychain.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using base::WideToUTF16;
using content::BrowserThread;
using crypto::MockAppleKeychain;
using internal_keychain_helpers::FormsMatchForMerge;
using internal_keychain_helpers::STRICT_FORM_MATCH;
using password_manager::CreatePasswordFormFromDataForTesting;
using password_manager::LoginDatabase;
using password_manager::PasswordFormData;
using password_manager::PasswordStore;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using password_manager::PasswordStoreConsumer;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::IsEmpty;
using testing::SizeIs;
using testing::WithArg;

namespace {

ACTION(QuitUIMessageLoop) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::MessageLoop::current()->QuitWhenIdle();
}

// From the mock's argument #0 of type const std::vector<PasswordForm*>& takes
// the first form and copies it to the form pointed to by |target_form_ptr|.
ACTION_P(SaveACopyOfFirstForm, target_form_ptr) {
  ASSERT_FALSE(arg0.empty());
  *target_form_ptr = *arg0[0];
}

void Noop() {}

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD1(OnGetPasswordStoreResultsConstRef,
               void(const std::vector<std::unique_ptr<PasswordForm>>&));

  // GMock cannot mock methods with move-only args.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    OnGetPasswordStoreResultsConstRef(results);
  }
};

// A LoginDatabase that simulates an Init() method that takes a long time.
class SlowToInitLoginDatabase : public password_manager::LoginDatabase {
 public:
  // Creates an instance whose Init() method will block until |event| is
  // signaled. |event| must outlive |this|.
  SlowToInitLoginDatabase(const base::FilePath& db_path,
                          base::WaitableEvent* event)
      : password_manager::LoginDatabase(db_path), event_(event) {}
  ~SlowToInitLoginDatabase() override {}

  // LoginDatabase:
  bool Init() override {
    event_->Wait();
    return password_manager::LoginDatabase::Init();
  }

 private:
  base::WaitableEvent* event_;

  DISALLOW_COPY_AND_ASSIGN(SlowToInitLoginDatabase);
};

#pragma mark -

// Macro to simplify calling CheckFormsAgainstExpectations with a useful label.
#define CHECK_FORMS(forms, expectations, i) \
  CheckFormsAgainstExpectations(forms, expectations, #forms, i)

// Ensures that the data in |forms| match |expectations|, causing test failures
// for any discrepencies.
// TODO(stuartmorgan): This is current order-dependent; ideally it shouldn't
// matter if |forms| and |expectations| are scrambled.
void CheckFormsAgainstExpectations(
    const std::vector<std::unique_ptr<PasswordForm>>& forms,
    const std::vector<PasswordFormData*>& expectations,

    const char* forms_label,
    unsigned int test_number) {
  EXPECT_EQ(expectations.size(), forms.size()) << forms_label << " in test "
                                               << test_number;
  if (expectations.size() != forms.size())
    return;

  for (unsigned int i = 0; i < expectations.size(); ++i) {
    SCOPED_TRACE(testing::Message() << forms_label << " in test " << test_number
                                    << ", item " << i);
    PasswordForm* form = forms[i].get();
    PasswordFormData* expectation = expectations[i];
    EXPECT_EQ(expectation->scheme, form->scheme);
    EXPECT_EQ(std::string(expectation->signon_realm), form->signon_realm);
    EXPECT_EQ(GURL(expectation->origin), form->origin);
    EXPECT_EQ(GURL(expectation->action), form->action);
    EXPECT_EQ(WideToUTF16(expectation->submit_element), form->submit_element);
    EXPECT_EQ(WideToUTF16(expectation->username_element),
              form->username_element);
    EXPECT_EQ(WideToUTF16(expectation->password_element),
              form->password_element);
    if (expectation->username_value) {
      EXPECT_EQ(WideToUTF16(expectation->username_value), form->username_value);
      EXPECT_EQ(WideToUTF16(expectation->username_value), form->display_name);
      EXPECT_TRUE(form->skip_zero_click);
      if (expectation->password_value &&
          wcscmp(expectation->password_value,
                 password_manager::kTestingFederatedLoginMarker) == 0) {
        EXPECT_TRUE(form->password_value.empty());
        EXPECT_EQ(
            url::Origin(GURL(password_manager::kTestingFederationUrlSpec)),
            form->federation_origin);
      } else {
        EXPECT_EQ(WideToUTF16(expectation->password_value),
                  form->password_value);
        EXPECT_TRUE(form->federation_origin.unique());
      }
    } else {
      EXPECT_TRUE(form->blacklisted_by_user);
    }
    EXPECT_EQ(expectation->preferred, form->preferred);
    EXPECT_DOUBLE_EQ(expectation->creation_time,
                     form->date_created.ToDoubleT());
    base::Time created = base::Time::FromDoubleT(expectation->creation_time);
    EXPECT_EQ(
        created + base::TimeDelta::FromDays(
                      password_manager::kTestingDaysAfterPasswordsAreSynced),
        form->date_synced);
    EXPECT_EQ(GURL(password_manager::kTestingIconUrlSpec), form->icon_url);
  }
}

PasswordStoreChangeList AddChangeForForm(const PasswordForm& form) {
  return PasswordStoreChangeList(
      1, PasswordStoreChange(PasswordStoreChange::ADD, form));
}

class PasswordStoreMacTestDelegate {
 public:
  PasswordStoreMacTestDelegate();
  ~PasswordStoreMacTestDelegate();

  PasswordStoreMac* store() { return store_.get(); }

  static void FinishAsyncProcessing();

 private:
  void Initialize();

  void ClosePasswordStore();

  base::FilePath test_login_db_file_path() const;

  base::MessageLoopForUI message_loop_;
  base::ScopedTempDir db_dir_;
  std::unique_ptr<LoginDatabase> login_db_;
  scoped_refptr<PasswordStoreMac> store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreMacTestDelegate);
};

PasswordStoreMacTestDelegate::PasswordStoreMacTestDelegate() {
  Initialize();
}

PasswordStoreMacTestDelegate::~PasswordStoreMacTestDelegate() {
  ClosePasswordStore();
}

void PasswordStoreMacTestDelegate::FinishAsyncProcessing() {
  base::RunLoop().RunUntilIdle();
}

void PasswordStoreMacTestDelegate::Initialize() {
  ASSERT_TRUE(db_dir_.CreateUniqueTempDir());

  // Ensure that LoginDatabase will use the mock keychain if it needs to
  // encrypt/decrypt a password.
  OSCryptMocker::SetUpWithSingleton();
  login_db_.reset(new LoginDatabase(test_login_db_file_path()));
  ASSERT_TRUE(login_db_->Init());

  // Create and initialize the password store.
  store_ = new PasswordStoreMac(base::ThreadTaskRunnerHandle::Get(),
                                base::ThreadTaskRunnerHandle::Get(),
                                base::WrapUnique(new MockAppleKeychain));
  store_->set_login_metadata_db(login_db_.get());
  store_->login_metadata_db()->set_clear_password_values(false);
}

void PasswordStoreMacTestDelegate::ClosePasswordStore() {
  store_->ShutdownOnUIThread();
  FinishAsyncProcessing();
  OSCryptMocker::TearDown();
}

base::FilePath PasswordStoreMacTestDelegate::test_login_db_file_path() const {
  return db_dir_.GetPath().Append(FILE_PATH_LITERAL("login.db"));
}

}  // namespace

namespace password_manager {

INSTANTIATE_TYPED_TEST_CASE_P(Mac,
                              PasswordStoreOriginTest,
                              PasswordStoreMacTestDelegate);

}  // namespace password_manager

#pragma mark -

class PasswordStoreMacInternalsTest : public testing::Test {
 public:
  void SetUp() override {
    MockAppleKeychain::KeychainTestData test_data[] = {
        // Basic HTML form.
        {kSecAuthenticationTypeHTMLForm, "some.domain.com",
         kSecProtocolTypeHTTP, NULL, 0, NULL, "20020601171500Z", "joe_user",
         "sekrit", false},
        // HTML form with path.
        {kSecAuthenticationTypeHTMLForm, "some.domain.com",
         kSecProtocolTypeHTTP, "/insecure.html", 0, NULL, "19991231235959Z",
         "joe_user", "sekrit", false},
        // Secure HTML form with path.
        {kSecAuthenticationTypeHTMLForm, "some.domain.com",
         kSecProtocolTypeHTTPS, "/secure.html", 0, NULL, "20100908070605Z",
         "secure_user", "password", false},
        // True negative item.
        {kSecAuthenticationTypeHTMLForm, "dont.remember.com",
         kSecProtocolTypeHTTP, NULL, 0, NULL, "20000101000000Z", "", "", true},
        // De-facto negative item, type one.
        {kSecAuthenticationTypeHTMLForm, "dont.remember.com",
         kSecProtocolTypeHTTP, NULL, 0, NULL, "20000101000000Z",
         "Password Not Stored", "", false},
        // De-facto negative item, type two.
        {kSecAuthenticationTypeHTMLForm, "dont.remember.com",
         kSecProtocolTypeHTTPS, NULL, 0, NULL, "20000101000000Z",
         "Password Not Stored", " ", false},
        // HTTP auth basic, with port and path.
        {kSecAuthenticationTypeHTTPBasic, "some.domain.com",
         kSecProtocolTypeHTTP, "/insecure.html", 4567, "low_security",
         "19980330100000Z", "basic_auth_user", "basic", false},
        // HTTP auth digest, secure.
        {kSecAuthenticationTypeHTTPDigest, "some.domain.com",
         kSecProtocolTypeHTTPS, NULL, 0, "high_security", "19980330100000Z",
         "digest_auth_user", "digest", false},
        // An FTP password with an invalid date, for edge-case testing.
        {kSecAuthenticationTypeDefault, "a.server.com", kSecProtocolTypeFTP,
         NULL, 0, NULL, "20010203040", "abc", "123", false},
        // Password for an Android application.
        {kSecAuthenticationTypeHTMLForm, "android://hash@com.domain.some/",
         kSecProtocolTypeHTTPS, "", 0, NULL, "20150515141312Z", "joe_user",
         "secret", false},
    };

    keychain_ = new MockAppleKeychain();

    for (unsigned int i = 0; i < arraysize(test_data); ++i) {
      keychain_->AddTestItem(test_data[i]);
    }
  }

  void TearDown() override {
    ExpectCreatesAndFreesBalanced();
    ExpectCreatorCodesSet();
    delete keychain_;
  }

 protected:
  // Causes a test failure unless everything returned from keychain_'s
  // ItemCopyAttributesAndData, SearchCreateFromAttributes, and SearchCopyNext
  // was correctly freed.
  void ExpectCreatesAndFreesBalanced() {
    EXPECT_EQ(0, keychain_->UnfreedSearchCount());
    EXPECT_EQ(0, keychain_->UnfreedKeychainItemCount());
    EXPECT_EQ(0, keychain_->UnfreedAttributeDataCount());
  }

  // Causes a test failure unless any Keychain items added during the test have
  // their creator code set.
  void ExpectCreatorCodesSet() {
    EXPECT_TRUE(keychain_->CreatorCodesSetForAddedItems());
  }

  MockAppleKeychain* keychain_;
};

#pragma mark -

TEST_F(PasswordStoreMacInternalsTest, TestKeychainToFormTranslation) {
  typedef struct {
    const PasswordForm::Scheme scheme;
    const char* signon_realm;
    const char* origin;
    const wchar_t* username;  // Set to NULL to check for a blacklist entry.
    const wchar_t* password;
    const int creation_year;
    const int creation_month;
    const int creation_day;
    const int creation_hour;
    const int creation_minute;
    const int creation_second;
  } TestExpectations;

  TestExpectations expected[] = {
      {PasswordForm::SCHEME_HTML, "http://some.domain.com/",
       "http://some.domain.com/", L"joe_user", L"sekrit", 2002, 6, 1, 17, 15,
       0},
      {PasswordForm::SCHEME_HTML, "http://some.domain.com/",
       "http://some.domain.com/insecure.html", L"joe_user", L"sekrit", 1999, 12,
       31, 23, 59, 59},
      {PasswordForm::SCHEME_HTML, "https://some.domain.com/",
       "https://some.domain.com/secure.html", L"secure_user", L"password", 2010,
       9, 8, 7, 6, 5},
      {PasswordForm::SCHEME_HTML, "http://dont.remember.com/",
       "http://dont.remember.com/", NULL, NULL, 2000, 1, 1, 0, 0, 0},
      {PasswordForm::SCHEME_HTML, "http://dont.remember.com/",
       "http://dont.remember.com/", NULL, NULL, 2000, 1, 1, 0, 0, 0},
      {PasswordForm::SCHEME_HTML, "https://dont.remember.com/",
       "https://dont.remember.com/", NULL, NULL, 2000, 1, 1, 0, 0, 0},
      {PasswordForm::SCHEME_BASIC, "http://some.domain.com:4567/low_security",
       "http://some.domain.com:4567/insecure.html", L"basic_auth_user",
       L"basic", 1998, 03, 30, 10, 00, 00},
      {PasswordForm::SCHEME_DIGEST, "https://some.domain.com/high_security",
       "https://some.domain.com/", L"digest_auth_user", L"digest", 1998, 3, 30,
       10, 0, 0},
      // This one gives us an invalid date, which we will treat as a "NULL" date
      // which is 1601.
      {PasswordForm::SCHEME_OTHER, "http://a.server.com/",
       "http://a.server.com/", L"abc", L"123", 1601, 1, 1, 0, 0, 0},
      {PasswordForm::SCHEME_HTML, "android://hash@com.domain.some/", "",
       L"joe_user", L"secret", 2015, 5, 15, 14, 13, 12},
  };

  for (unsigned int i = 0; i < arraysize(expected); ++i) {
    SCOPED_TRACE(testing::Message("In iteration ") << i);
    // Create our fake KeychainItemRef; see MockAppleKeychain docs.
    SecKeychainItemRef keychain_item =
        reinterpret_cast<SecKeychainItemRef>(i + 1);
    PasswordForm form;
    bool parsed = internal_keychain_helpers::FillPasswordFormFromKeychainItem(
        *keychain_, keychain_item, &form, true);

    EXPECT_TRUE(parsed);

    EXPECT_EQ(expected[i].scheme, form.scheme);
    EXPECT_EQ(GURL(expected[i].origin), form.origin);
    EXPECT_EQ(std::string(expected[i].signon_realm), form.signon_realm);
    if (expected[i].username) {
      EXPECT_EQ(WideToUTF16(expected[i].username), form.username_value);
      EXPECT_EQ(WideToUTF16(expected[i].password), form.password_value);
      EXPECT_FALSE(form.blacklisted_by_user);
    } else {
      EXPECT_TRUE(form.blacklisted_by_user);
    }
    base::Time::Exploded exploded_time;
    form.date_created.UTCExplode(&exploded_time);
    EXPECT_EQ(expected[i].creation_year, exploded_time.year);
    EXPECT_EQ(expected[i].creation_month, exploded_time.month);
    EXPECT_EQ(expected[i].creation_day, exploded_time.day_of_month);
    EXPECT_EQ(expected[i].creation_hour, exploded_time.hour);
    EXPECT_EQ(expected[i].creation_minute, exploded_time.minute);
    EXPECT_EQ(expected[i].creation_second, exploded_time.second);
  }

  {
    // Use an invalid ref, to make sure errors are reported.
    SecKeychainItemRef keychain_item = reinterpret_cast<SecKeychainItemRef>(99);
    PasswordForm form;
    bool parsed = internal_keychain_helpers::FillPasswordFormFromKeychainItem(
        *keychain_, keychain_item, &form, true);
    EXPECT_FALSE(parsed);
  }
}

TEST_F(PasswordStoreMacInternalsTest, TestKeychainSearch) {
  struct TestDataAndExpectation {
    const PasswordFormData data;
    const size_t expected_fill_matches;
    const size_t expected_merge_matches;
  };
  // Most fields are left blank because we don't care about them for searching.
  TestDataAndExpectation test_data[] = {
      // An HTML form we've seen.
      {{PasswordForm::SCHEME_HTML, "http://some.domain.com/", NULL, NULL, NULL,
        NULL, NULL, L"joe_user", NULL, false, 0},
       2,
       2},
      {{PasswordForm::SCHEME_HTML, "http://some.domain.com/", NULL, NULL, NULL,
        NULL, NULL, L"wrong_user", NULL, false, 0},
       2,
       0},
      // An HTML form we haven't seen
      {{PasswordForm::SCHEME_HTML, "http://www.unseendomain.com/", NULL, NULL,
        NULL, NULL, NULL, L"joe_user", NULL, false, 0},
       0,
       0},
      // Basic auth that should match.
      {{PasswordForm::SCHEME_BASIC, "http://some.domain.com:4567/low_security",
        NULL, NULL, NULL, NULL, NULL, L"basic_auth_user", NULL, false, 0},
       1,
       1},
      // Basic auth with the wrong port.
      {{PasswordForm::SCHEME_BASIC, "http://some.domain.com:1111/low_security",
        NULL, NULL, NULL, NULL, NULL, L"basic_auth_user", NULL, false, 0},
       0,
       0},
      // Digest auth we've saved under https, visited with http.
      {{PasswordForm::SCHEME_DIGEST, "http://some.domain.com/high_security",
        NULL, NULL, NULL, NULL, NULL, L"digest_auth_user", NULL, false, 0},
       0,
       0},
      // Digest auth that should match.
      {{PasswordForm::SCHEME_DIGEST, "https://some.domain.com/high_security",
        NULL, NULL, NULL, NULL, NULL, L"wrong_user", NULL, false, 0},
       1,
       0},
      // Digest auth with the wrong domain.
      {{PasswordForm::SCHEME_DIGEST, "https://some.domain.com/other_domain",
        NULL, NULL, NULL, NULL, NULL, L"digest_auth_user", NULL, false, 0},
       0,
       0},
      // Android credentials (both legacy ones with origin, and without).
      {{PasswordForm::SCHEME_HTML, "android://hash@com.domain.some/",
        "android://hash@com.domain.some/", NULL, NULL, NULL, NULL, L"joe_user",
        NULL, false, 0},
       1,
       1},
      {{PasswordForm::SCHEME_HTML, "android://hash@com.domain.some/", NULL,
        NULL, NULL, NULL, NULL, L"joe_user", NULL, false, 0},
       1,
       1},
      // Federated logins do not have a corresponding Keychain entry, and should
      // not match the username/password stored for the same application. Note
      // that it will match for filling, however, because that part does not
      // know
      // that it is a federated login.
      {{PasswordForm::SCHEME_HTML, "android://hash@com.domain.some/", NULL,
        NULL, NULL, NULL, NULL, L"joe_user",
        password_manager::kTestingFederatedLoginMarker, false, 0},
       1,
       0},
      /// Garbage forms should have no matches.
      {{PasswordForm::SCHEME_HTML, "foo/bar/baz", NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, false, 0},
       0,
       0},
  };

  MacKeychainPasswordFormAdapter keychain_adapter(keychain_);
  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_);
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);
  for (unsigned int i = 0; i < arraysize(test_data); ++i) {
    std::unique_ptr<PasswordForm> query_form =
        CreatePasswordFormFromDataForTesting(test_data[i].data);

    // Check matches treating the form as a fill target.
    std::vector<std::unique_ptr<PasswordForm>> matching_items =
        keychain_adapter.PasswordsFillingForm(query_form->signon_realm,
                                              query_form->scheme);
    EXPECT_EQ(test_data[i].expected_fill_matches, matching_items.size());

    // Check matches treating the form as a merging target.
    EXPECT_EQ(test_data[i].expected_merge_matches > 0,
              keychain_adapter.HasPasswordsMergeableWithForm(*query_form));
    std::vector<SecKeychainItemRef> keychain_items;
    std::vector<internal_keychain_helpers::ItemFormPair> item_form_pairs =
        internal_keychain_helpers::
            ExtractAllKeychainItemAttributesIntoPasswordForms(&keychain_items,
                                                              *keychain_);
    matching_items =
        internal_keychain_helpers::ExtractPasswordsMergeableWithForm(
            *keychain_, item_form_pairs, *query_form);
    EXPECT_EQ(test_data[i].expected_merge_matches, matching_items.size());
    for (std::vector<SecKeychainItemRef>::iterator i = keychain_items.begin();
         i != keychain_items.end(); ++i) {
      keychain_->Free(*i);
    }

    // None of the pre-seeded items are owned by us, so none should match an
    // owned-passwords-only search.
    matching_items = owned_keychain_adapter.PasswordsFillingForm(
        query_form->signon_realm, query_form->scheme);
    EXPECT_EQ(0U, matching_items.size());
  }
}

// Changes just the origin path of |form|.
static void SetPasswordFormPath(PasswordForm* form, const char* path) {
  GURL::Replacements replacement;
  std::string new_value(path);
  replacement.SetPathStr(new_value);
  form->origin = form->origin.ReplaceComponents(replacement);
}

// Changes just the signon_realm port of |form|.
static void SetPasswordFormPort(PasswordForm* form, const char* port) {
  GURL::Replacements replacement;
  std::string new_value(port);
  replacement.SetPortStr(new_value);
  GURL signon_gurl = GURL(form->signon_realm);
  form->signon_realm = signon_gurl.ReplaceComponents(replacement).spec();
}

// Changes just the signon_ream auth realm of |form|.
static void SetPasswordFormRealm(PasswordForm* form, const char* realm) {
  GURL::Replacements replacement;
  std::string new_value(realm);
  replacement.SetPathStr(new_value);
  GURL signon_gurl = GURL(form->signon_realm);
  form->signon_realm = signon_gurl.ReplaceComponents(replacement).spec();
}

TEST_F(PasswordStoreMacInternalsTest, TestKeychainExactSearch) {
  MacKeychainPasswordFormAdapter keychain_adapter(keychain_);

  PasswordFormData base_form_data[] = {
      {PasswordForm::SCHEME_HTML, "http://some.domain.com/",
       "http://some.domain.com/insecure.html", NULL, NULL, NULL, NULL,
       L"joe_user", NULL, true, 0},
      {PasswordForm::SCHEME_BASIC, "http://some.domain.com:4567/low_security",
       "http://some.domain.com:4567/insecure.html", NULL, NULL, NULL, NULL,
       L"basic_auth_user", NULL, true, 0},
      {PasswordForm::SCHEME_DIGEST, "https://some.domain.com/high_security",
       "https://some.domain.com", NULL, NULL, NULL, NULL, L"digest_auth_user",
       NULL, true, 0},
  };

  for (unsigned int i = 0; i < arraysize(base_form_data); ++i) {
    // Create a base form and make sure we find a match.
    std::unique_ptr<PasswordForm> base_form =
        CreatePasswordFormFromDataForTesting(base_form_data[i]);
    EXPECT_TRUE(keychain_adapter.HasPasswordsMergeableWithForm(*base_form));
    EXPECT_TRUE(keychain_adapter.HasPasswordExactlyMatchingForm(*base_form));

    // Make sure that the matching isn't looser than it should be by checking
    // that slightly altered forms don't match.
    std::vector<std::unique_ptr<PasswordForm>> modified_forms;

    modified_forms.push_back(base::MakeUnique<PasswordForm>(*base_form));
    modified_forms.back()->username_value = ASCIIToUTF16("wrong_user");

    modified_forms.push_back(base::MakeUnique<PasswordForm>(*base_form));
    SetPasswordFormPath(modified_forms.back().get(), "elsewhere.html");

    modified_forms.push_back(base::MakeUnique<PasswordForm>(*base_form));
    modified_forms.back()->scheme = PasswordForm::SCHEME_OTHER;

    modified_forms.push_back(base::MakeUnique<PasswordForm>(*base_form));
    SetPasswordFormPort(modified_forms.back().get(), "1234");

    modified_forms.push_back(base::MakeUnique<PasswordForm>(*base_form));
    modified_forms.back()->blacklisted_by_user = true;

    if (base_form->scheme == PasswordForm::SCHEME_BASIC ||
        base_form->scheme == PasswordForm::SCHEME_DIGEST) {
      modified_forms.push_back(base::MakeUnique<PasswordForm>(*base_form));
      SetPasswordFormRealm(modified_forms.back().get(), "incorrect");
    }

    for (unsigned int j = 0; j < modified_forms.size(); ++j) {
      bool match =
          keychain_adapter.HasPasswordExactlyMatchingForm(*modified_forms[j]);
      EXPECT_FALSE(match) << "In modified version " << j << " of base form "
                          << i;
    }
  }
}

TEST_F(PasswordStoreMacInternalsTest, TestKeychainAdd) {
  struct TestDataAndExpectation {
    PasswordFormData data;
    bool should_succeed;
  };
  TestDataAndExpectation test_data[] = {
      // Test a variety of scheme/port/protocol/path variations.
      {{PasswordForm::SCHEME_HTML, "http://web.site.com/",
        "http://web.site.com/path/to/page.html", NULL, NULL, NULL, NULL,
        L"anonymous", L"knock-knock", false, 0},
       true},
      {{PasswordForm::SCHEME_HTML, "https://web.site.com/",
        "https://web.site.com/", NULL, NULL, NULL, NULL, L"admin", L"p4ssw0rd",
        false, 0},
       true},
      {{PasswordForm::SCHEME_BASIC, "http://a.site.com:2222/therealm",
        "http://a.site.com:2222/", NULL, NULL, NULL, NULL, L"username",
        L"password", false, 0},
       true},
      {{PasswordForm::SCHEME_DIGEST, "https://digest.site.com/differentrealm",
        "https://digest.site.com/secure.html", NULL, NULL, NULL, NULL,
        L"testname", L"testpass", false, 0},
       true},
      // Test that Android credentials can be stored. Also check the legacy form
      // when |origin| was still filled with the Android URI (and not left
      // empty).
      {{PasswordForm::SCHEME_HTML, "android://hash@com.example.alpha/", "",
        NULL, NULL, NULL, NULL, L"joe_user", L"password", false, 0},
       true},
      {{PasswordForm::SCHEME_HTML, "android://hash@com.example.beta/",
        "android://hash@com.example.beta/", NULL, NULL, NULL, NULL,
        L"jane_user", L"password2", false, 0},
       true},
      // Make sure that garbage forms are rejected.
      {{PasswordForm::SCHEME_HTML, "gobbledygook", "gobbledygook", NULL, NULL,
        NULL, NULL, L"anonymous", L"knock-knock", false, 0},
       false},
      // Test that failing to update a duplicate (forced using the magic failure
      // password; see MockAppleKeychain::ItemModifyAttributesAndData) is
      // reported.
      {{PasswordForm::SCHEME_HTML, "http://some.domain.com",
        "http://some.domain.com/insecure.html", NULL, NULL, NULL, NULL,
        L"joe_user", L"fail_me", false, 0},
       false},
  };

  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_);
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);

  for (unsigned int i = 0; i < arraysize(test_data); ++i) {
    std::unique_ptr<PasswordForm> in_form =
        CreatePasswordFormFromDataForTesting(test_data[i].data);
    bool add_succeeded = owned_keychain_adapter.AddPassword(*in_form);
    EXPECT_EQ(test_data[i].should_succeed, add_succeeded);
    if (add_succeeded) {
      EXPECT_TRUE(
          owned_keychain_adapter.HasPasswordsMergeableWithForm(*in_form));
      EXPECT_TRUE(
          owned_keychain_adapter.HasPasswordExactlyMatchingForm(*in_form));
    }
  }

  // Test that adding duplicate item updates the existing item.
  // TODO(engedy): Add a test to verify that updating Android credentials work.
  // See: https://crbug.com/476851.
  {
    PasswordFormData data = {PasswordForm::SCHEME_HTML,
                             "http://some.domain.com",
                             "http://some.domain.com/insecure.html",
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             L"joe_user",
                             L"updated_password",
                             false,
                             0};
    std::unique_ptr<PasswordForm> update_form =
        CreatePasswordFormFromDataForTesting(data);
    MacKeychainPasswordFormAdapter keychain_adapter(keychain_);
    EXPECT_TRUE(keychain_adapter.AddPassword(*update_form));
    SecKeychainItemRef keychain_item = reinterpret_cast<SecKeychainItemRef>(2);
    PasswordForm stored_form;
    internal_keychain_helpers::FillPasswordFormFromKeychainItem(
        *keychain_, keychain_item, &stored_form, true);
    EXPECT_EQ(update_form->password_value, stored_form.password_value);
  }
}

TEST_F(PasswordStoreMacInternalsTest, TestKeychainRemove) {
  struct TestDataAndExpectation {
    PasswordFormData data;
    bool should_succeed;
  };
  TestDataAndExpectation test_data[] = {
      // Test deletion of an item that we add.
      {{PasswordForm::SCHEME_HTML, "http://web.site.com/",
        "http://web.site.com/path/to/page.html", NULL, NULL, NULL, NULL,
        L"anonymous", L"knock-knock", false, 0},
       true},
      // Test that Android credentials can be removed. Also check the legacy
      // case when |origin| was still filled with the Android URI (and not left
      // empty).
      {{PasswordForm::SCHEME_HTML, "android://hash@com.example.alpha/", "",
        NULL, NULL, NULL, NULL, L"joe_user", L"secret", false, 0},
       true},
      {{PasswordForm::SCHEME_HTML, "android://hash@com.example.beta/",
        "android://hash@com.example.beta/", NULL, NULL, NULL, NULL,
        L"jane_user", L"secret", false, 0},
       true},
      // Make sure we don't delete items we don't own.
      {{PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/insecure.html", NULL, NULL, NULL, NULL,
        L"joe_user", NULL, true, 0},
       false},
  };

  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_);
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);

  // Add our test items (except the last one) so that we can delete them.
  for (unsigned int i = 0; i + 1 < arraysize(test_data); ++i) {
    std::unique_ptr<PasswordForm> add_form =
        CreatePasswordFormFromDataForTesting(test_data[i].data);
    EXPECT_TRUE(owned_keychain_adapter.AddPassword(*add_form));
  }

  for (unsigned int i = 0; i < arraysize(test_data); ++i) {
    std::unique_ptr<PasswordForm> form =
        CreatePasswordFormFromDataForTesting(test_data[i].data);
    EXPECT_EQ(test_data[i].should_succeed,
              owned_keychain_adapter.RemovePassword(*form));

    MacKeychainPasswordFormAdapter keychain_adapter(keychain_);
    bool match = keychain_adapter.HasPasswordExactlyMatchingForm(*form);
    EXPECT_EQ(test_data[i].should_succeed, !match);
  }
}

TEST_F(PasswordStoreMacInternalsTest, TestFormMatch) {
  PasswordForm base_form;
  base_form.signon_realm = std::string("http://some.domain.com/");
  base_form.origin = GURL("http://some.domain.com/page.html");
  base_form.username_value = ASCIIToUTF16("joe_user");

  {
    // Check that everything unimportant can be changed.
    PasswordForm different_form(base_form);
    different_form.username_element = ASCIIToUTF16("username");
    different_form.submit_element = ASCIIToUTF16("submit");
    different_form.username_element = ASCIIToUTF16("password");
    different_form.password_value = ASCIIToUTF16("sekrit");
    different_form.action = GURL("http://some.domain.com/action.cgi");
    different_form.preferred = true;
    different_form.date_created = base::Time::Now();
    EXPECT_TRUE(
        FormsMatchForMerge(base_form, different_form, STRICT_FORM_MATCH));

    // Check that path differences don't prevent a match.
    base_form.origin = GURL("http://some.domain.com/other_page.html");
    EXPECT_TRUE(
        FormsMatchForMerge(base_form, different_form, STRICT_FORM_MATCH));
  }

  // Check that any one primary key changing is enough to prevent matching.
  {
    PasswordForm different_form(base_form);
    different_form.scheme = PasswordForm::SCHEME_DIGEST;
    EXPECT_FALSE(
        FormsMatchForMerge(base_form, different_form, STRICT_FORM_MATCH));
  }
  {
    PasswordForm different_form(base_form);
    different_form.signon_realm = std::string("http://some.domain.com:8080/");
    EXPECT_FALSE(
        FormsMatchForMerge(base_form, different_form, STRICT_FORM_MATCH));
  }
  {
    PasswordForm different_form(base_form);
    different_form.username_value = ASCIIToUTF16("john.doe");
    EXPECT_FALSE(
        FormsMatchForMerge(base_form, different_form, STRICT_FORM_MATCH));
  }
  {
    PasswordForm different_form(base_form);
    different_form.blacklisted_by_user = true;
    EXPECT_FALSE(
        FormsMatchForMerge(base_form, different_form, STRICT_FORM_MATCH));
  }

  // Blacklist forms should *never* match for merging, even when identical
  // (and certainly not when only one is a blacklist entry).
  {
    PasswordForm form_a(base_form);
    form_a.blacklisted_by_user = true;
    PasswordForm form_b(form_a);
    EXPECT_FALSE(FormsMatchForMerge(form_a, form_b, STRICT_FORM_MATCH));
  }

  // Federated login forms should never match for merging either.
  {
    PasswordForm form_b(base_form);
    form_b.federation_origin =
        url::Origin(GURL(password_manager::kTestingFederationUrlSpec));
    EXPECT_FALSE(FormsMatchForMerge(base_form, form_b, STRICT_FORM_MATCH));
    EXPECT_FALSE(FormsMatchForMerge(form_b, base_form, STRICT_FORM_MATCH));
    EXPECT_FALSE(FormsMatchForMerge(form_b, form_b, STRICT_FORM_MATCH));
  }
}

TEST_F(PasswordStoreMacInternalsTest, TestFormMerge) {
  // Set up a bunch of test data to use in varying combinations.
  PasswordFormData keychain_user_1 = {PasswordForm::SCHEME_HTML,
                                      "http://some.domain.com/",
                                      "http://some.domain.com/",
                                      "",
                                      L"",
                                      L"",
                                      L"",
                                      L"joe_user",
                                      L"sekrit",
                                      false,
                                      1010101010};
  PasswordFormData keychain_user_1_with_path = {
      PasswordForm::SCHEME_HTML,
      "http://some.domain.com/",
      "http://some.domain.com/page.html",
      "",
      L"",
      L"",
      L"",
      L"joe_user",
      L"otherpassword",
      false,
      1010101010};
  PasswordFormData keychain_user_2 = {PasswordForm::SCHEME_HTML,
                                      "http://some.domain.com/",
                                      "http://some.domain.com/",
                                      "",
                                      L"",
                                      L"",
                                      L"",
                                      L"john.doe",
                                      L"sesame",
                                      false,
                                      958739876};
  PasswordFormData keychain_blacklist = {PasswordForm::SCHEME_HTML,
                                         "http://some.domain.com/",
                                         "http://some.domain.com/",
                                         "",
                                         L"",
                                         L"",
                                         L"",
                                         NULL,
                                         NULL,
                                         false,
                                         1010101010};
  PasswordFormData keychain_android = {PasswordForm::SCHEME_HTML,
                                       "android://hash@com.domain.some/",
                                       "",
                                       "",
                                       L"",
                                       L"",
                                       L"",
                                       L"joe_user",
                                       L"secret",
                                       false,
                                       1234567890};

  PasswordFormData db_user_1 = {PasswordForm::SCHEME_HTML,
                                "http://some.domain.com/",
                                "http://some.domain.com/",
                                "http://some.domain.com/action.cgi",
                                L"submit",
                                L"username",
                                L"password",
                                L"joe_user",
                                L"",
                                true,
                                1212121212};
  PasswordFormData db_user_1_with_path = {
      PasswordForm::SCHEME_HTML,
      "http://some.domain.com/",
      "http://some.domain.com/page.html",
      "http://some.domain.com/handlepage.cgi",
      L"submit",
      L"username",
      L"password",
      L"joe_user",
      L"",
      true,
      1234567890};
  PasswordFormData db_user_3_with_path = {
      PasswordForm::SCHEME_HTML,
      "http://some.domain.com/",
      "http://some.domain.com/page.html",
      "http://some.domain.com/handlepage.cgi",
      L"submit",
      L"username",
      L"password",
      L"second-account",
      L"",
      true,
      1240000000};
  PasswordFormData database_blacklist_with_path = {
      PasswordForm::SCHEME_HTML,
      "http://some.domain.com/",
      "http://some.domain.com/path.html",
      "http://some.domain.com/action.cgi",
      L"submit",
      L"username",
      L"password",
      NULL,
      NULL,
      true,
      1212121212};
  PasswordFormData db_android = {PasswordForm::SCHEME_HTML,
                                 "android://hash@com.domain.some/",
                                 "android://hash@com.domain.some/",
                                 "",
                                 L"",
                                 L"",
                                 L"",
                                 L"joe_user",
                                 L"",
                                 false,
                                 1234567890};
  PasswordFormData db_federated = {
      PasswordForm::SCHEME_HTML,
      "android://hash@com.domain.some/",
      "android://hash@com.domain.some/",
      "",
      L"",
      L"",
      L"",
      L"joe_user",
      password_manager::kTestingFederatedLoginMarker,
      false,
      3434343434};

  PasswordFormData merged_user_1 = {PasswordForm::SCHEME_HTML,
                                    "http://some.domain.com/",
                                    "http://some.domain.com/",
                                    "http://some.domain.com/action.cgi",
                                    L"submit",
                                    L"username",
                                    L"password",
                                    L"joe_user",
                                    L"sekrit",
                                    true,
                                    1212121212};
  PasswordFormData merged_user_1_with_db_path = {
      PasswordForm::SCHEME_HTML,
      "http://some.domain.com/",
      "http://some.domain.com/page.html",
      "http://some.domain.com/handlepage.cgi",
      L"submit",
      L"username",
      L"password",
      L"joe_user",
      L"sekrit",
      true,
      1234567890};
  PasswordFormData merged_user_1_with_both_paths = {
      PasswordForm::SCHEME_HTML,
      "http://some.domain.com/",
      "http://some.domain.com/page.html",
      "http://some.domain.com/handlepage.cgi",
      L"submit",
      L"username",
      L"password",
      L"joe_user",
      L"otherpassword",
      true,
      1234567890};
  PasswordFormData merged_android = {PasswordForm::SCHEME_HTML,
                                     "android://hash@com.domain.some/",
                                     "android://hash@com.domain.some/",
                                     "",
                                     L"",
                                     L"",
                                     L"",
                                     L"joe_user",
                                     L"secret",
                                     false,
                                     1234567890};

  // Build up the big multi-dimensional array of data sets that will actually
  // drive the test. Use vectors rather than arrays so that initialization is
  // simple.
  enum {
    KEYCHAIN_INPUT = 0,
    DATABASE_INPUT,
    MERGE_OUTPUT,
    KEYCHAIN_OUTPUT,
    DATABASE_OUTPUT,
    MERGE_IO_ARRAY_COUNT  // termination marker
  };
  const unsigned int kTestCount = 5;
  std::vector<std::vector<std::vector<PasswordFormData*>>> test_data(
      MERGE_IO_ARRAY_COUNT, std::vector<std::vector<PasswordFormData*>>(
                                kTestCount, std::vector<PasswordFormData*>()));
  unsigned int current_test = 0;

  // Test a merge with a few accounts in both systems, with partial overlap.
  CHECK(current_test < kTestCount);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_user_1);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_user_2);
  test_data[DATABASE_INPUT][current_test].push_back(&db_user_1);
  test_data[DATABASE_INPUT][current_test].push_back(&db_user_1_with_path);
  test_data[DATABASE_INPUT][current_test].push_back(&db_user_3_with_path);
  test_data[MERGE_OUTPUT][current_test].push_back(&merged_user_1);
  test_data[MERGE_OUTPUT][current_test].push_back(&merged_user_1_with_db_path);
  test_data[KEYCHAIN_OUTPUT][current_test].push_back(&keychain_user_2);
  test_data[DATABASE_OUTPUT][current_test].push_back(&db_user_3_with_path);

  // Test a merge where Chrome has a blacklist entry, and the keychain has
  // a stored account.
  ++current_test;
  CHECK(current_test < kTestCount);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_user_1);
  test_data[DATABASE_INPUT][current_test].push_back(
      &database_blacklist_with_path);
  // We expect both to be present because a blacklist could be specific to a
  // subpath, and we want access to the password on other paths.
  test_data[MERGE_OUTPUT][current_test].push_back(
      &database_blacklist_with_path);
  test_data[KEYCHAIN_OUTPUT][current_test].push_back(&keychain_user_1);

  // Test a merge where Chrome has an account, and Keychain has a blacklist
  // (from another browser) and the Chrome password data.
  ++current_test;
  CHECK(current_test < kTestCount);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_blacklist);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_user_1);
  test_data[DATABASE_INPUT][current_test].push_back(&db_user_1);
  test_data[MERGE_OUTPUT][current_test].push_back(&merged_user_1);
  test_data[KEYCHAIN_OUTPUT][current_test].push_back(&keychain_blacklist);

  // Test that matches are done using exact path when possible.
  ++current_test;
  CHECK(current_test < kTestCount);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_user_1);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_user_1_with_path);
  test_data[DATABASE_INPUT][current_test].push_back(&db_user_1);
  test_data[DATABASE_INPUT][current_test].push_back(&db_user_1_with_path);
  test_data[MERGE_OUTPUT][current_test].push_back(&merged_user_1);
  test_data[MERGE_OUTPUT][current_test].push_back(
      &merged_user_1_with_both_paths);

  // Test that Android credentails are matched correctly and that federated
  // credentials are not tried to be matched with a Keychain item.
  ++current_test;
  CHECK(current_test < kTestCount);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_android);
  test_data[DATABASE_INPUT][current_test].push_back(&db_federated);
  test_data[DATABASE_INPUT][current_test].push_back(&db_android);
  test_data[MERGE_OUTPUT][current_test].push_back(&db_federated);
  test_data[MERGE_OUTPUT][current_test].push_back(&merged_android);

  for (unsigned int test_case = 0; test_case <= current_test; ++test_case) {
    std::vector<std::unique_ptr<PasswordForm>> keychain_forms;
    for (std::vector<PasswordFormData*>::iterator i =
             test_data[KEYCHAIN_INPUT][test_case].begin();
         i != test_data[KEYCHAIN_INPUT][test_case].end(); ++i) {
      keychain_forms.push_back(CreatePasswordFormFromDataForTesting(*(*i)));
    }
    std::vector<std::unique_ptr<PasswordForm>> database_forms;
    for (std::vector<PasswordFormData*>::iterator i =
             test_data[DATABASE_INPUT][test_case].begin();
         i != test_data[DATABASE_INPUT][test_case].end(); ++i) {
      database_forms.push_back(CreatePasswordFormFromDataForTesting(*(*i)));
    }

    std::vector<std::unique_ptr<PasswordForm>> merged_forms;
    internal_keychain_helpers::MergePasswordForms(
        &keychain_forms, &database_forms, &merged_forms);

    CHECK_FORMS(keychain_forms, test_data[KEYCHAIN_OUTPUT][test_case],
                test_case);
    CHECK_FORMS(database_forms, test_data[DATABASE_OUTPUT][test_case],
                test_case);
    CHECK_FORMS(merged_forms, test_data[MERGE_OUTPUT][test_case], test_case);
  }
}

TEST_F(PasswordStoreMacInternalsTest, TestPasswordBulkLookup) {
  PasswordFormData db_data[] = {
      {PasswordForm::SCHEME_HTML, "http://some.domain.com/",
       "http://some.domain.com/", "http://some.domain.com/action.cgi",
       L"submit", L"username", L"password", L"joe_user", L"", true, 1212121212},
      {PasswordForm::SCHEME_HTML, "http://some.domain.com/",
       "http://some.domain.com/page.html",
       "http://some.domain.com/handlepage.cgi", L"submit", L"username",
       L"password", L"joe_user", L"", true, 1234567890},
      {PasswordForm::SCHEME_HTML, "http://some.domain.com/",
       "http://some.domain.com/page.html",
       "http://some.domain.com/handlepage.cgi", L"submit", L"username",
       L"password", L"second-account", L"", true, 1240000000},
      {PasswordForm::SCHEME_HTML, "http://dont.remember.com/",
       "http://dont.remember.com/", "http://dont.remember.com/handlepage.cgi",
       L"submit", L"username", L"password", L"joe_user", L"", true, 1240000000},
      {PasswordForm::SCHEME_HTML, "http://some.domain.com/",
       "http://some.domain.com/path.html", "http://some.domain.com/action.cgi",
       L"submit", L"username", L"password", NULL, NULL, true, 1212121212},
  };
  std::vector<std::unique_ptr<PasswordForm>> database_forms;
  for (const PasswordFormData& form_data : db_data) {
    database_forms.push_back(CreatePasswordFormFromDataForTesting(form_data));
  }
  std::vector<std::unique_ptr<PasswordForm>> merged_forms;
  internal_keychain_helpers::GetPasswordsForForms(*keychain_, &database_forms,
                                                  &merged_forms);
  EXPECT_EQ(2U, database_forms.size());
  ASSERT_EQ(3U, merged_forms.size());
  EXPECT_EQ(ASCIIToUTF16("sekrit"), merged_forms[0]->password_value);
  EXPECT_EQ(ASCIIToUTF16("sekrit"), merged_forms[1]->password_value);
  EXPECT_TRUE(merged_forms[2]->blacklisted_by_user);
}

TEST_F(PasswordStoreMacInternalsTest, TestBlacklistedFiltering) {
  PasswordFormData db_data[] = {
      {PasswordForm::SCHEME_HTML, "http://dont.remember.com/",
       "http://dont.remember.com/", "http://dont.remember.com/handlepage.cgi",
       L"submit", L"username", L"password", L"joe_user", L"non_empty_password",
       true, 1240000000},
      {PasswordForm::SCHEME_HTML, "https://dont.remember.com/",
       "https://dont.remember.com/",
       "https://dont.remember.com/handlepage_secure.cgi", L"submit",
       L"username", L"password", L"joe_user", L"non_empty_password", true,
       1240000000},
  };
  std::vector<std::unique_ptr<PasswordForm>> database_forms;
  for (const PasswordFormData& form_data : db_data) {
    database_forms.push_back(CreatePasswordFormFromDataForTesting(form_data));
  }
  std::vector<std::unique_ptr<PasswordForm>> merged_forms;
  internal_keychain_helpers::GetPasswordsForForms(*keychain_, &database_forms,
                                                  &merged_forms);
  EXPECT_EQ(2U, database_forms.size());
  ASSERT_EQ(0U, merged_forms.size());
}

TEST_F(PasswordStoreMacInternalsTest, TestFillPasswordFormFromKeychainItem) {
  // When |extract_password_data| is false, the password field must be empty,
  // and |blacklisted_by_user| must be false.
  SecKeychainItemRef keychain_item = reinterpret_cast<SecKeychainItemRef>(1);
  PasswordForm form_without_extracted_password;
  bool parsed = internal_keychain_helpers::FillPasswordFormFromKeychainItem(
      *keychain_, keychain_item, &form_without_extracted_password,
      false);  // Do not extract password.
  EXPECT_TRUE(parsed);
  ASSERT_TRUE(form_without_extracted_password.password_value.empty());
  ASSERT_FALSE(form_without_extracted_password.blacklisted_by_user);

  // When |extract_password_data| is true and the keychain entry has a non-empty
  // password, the password field must be non-empty, and the value of
  // |blacklisted_by_user| must be false.
  keychain_item = reinterpret_cast<SecKeychainItemRef>(1);
  PasswordForm form_with_extracted_password;
  parsed = internal_keychain_helpers::FillPasswordFormFromKeychainItem(
      *keychain_, keychain_item, &form_with_extracted_password,
      true);  // Extract password.
  EXPECT_TRUE(parsed);
  ASSERT_EQ(ASCIIToUTF16("sekrit"),
            form_with_extracted_password.password_value);
  ASSERT_FALSE(form_with_extracted_password.blacklisted_by_user);

  // When |extract_password_data| is true and the keychain entry has an empty
  // username and password (""), the password field must be empty, and the value
  // of |blacklisted_by_user| must be true.
  keychain_item = reinterpret_cast<SecKeychainItemRef>(4);
  PasswordForm negative_form;
  parsed = internal_keychain_helpers::FillPasswordFormFromKeychainItem(
      *keychain_, keychain_item, &negative_form,
      true);  // Extract password.
  EXPECT_TRUE(parsed);
  ASSERT_TRUE(negative_form.username_value.empty());
  ASSERT_TRUE(negative_form.password_value.empty());
  ASSERT_TRUE(negative_form.blacklisted_by_user);

  // When |extract_password_data| is true and the keychain entry has an empty
  // password (""), the password field must be empty (""), and the value of
  // |blacklisted_by_user| must be true.
  keychain_item = reinterpret_cast<SecKeychainItemRef>(5);
  PasswordForm form_with_empty_password_a;
  parsed = internal_keychain_helpers::FillPasswordFormFromKeychainItem(
      *keychain_, keychain_item, &form_with_empty_password_a,
      true);  // Extract password.
  EXPECT_TRUE(parsed);
  ASSERT_TRUE(form_with_empty_password_a.password_value.empty());
  ASSERT_TRUE(form_with_empty_password_a.blacklisted_by_user);

  // When |extract_password_data| is true and the keychain entry has a single
  // space password (" "), the password field must be a single space (" "), and
  // the value of |blacklisted_by_user| must be true.
  keychain_item = reinterpret_cast<SecKeychainItemRef>(6);
  PasswordForm form_with_empty_password_b;
  parsed = internal_keychain_helpers::FillPasswordFormFromKeychainItem(
      *keychain_, keychain_item, &form_with_empty_password_b,
      true);  // Extract password.
  EXPECT_TRUE(parsed);
  ASSERT_EQ(ASCIIToUTF16(" "), form_with_empty_password_b.password_value);
  ASSERT_TRUE(form_with_empty_password_b.blacklisted_by_user);
}

TEST_F(PasswordStoreMacInternalsTest, TestPasswordGetAll) {
  MacKeychainPasswordFormAdapter keychain_adapter(keychain_);
  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_);
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);

  // Add a few passwords of various types so that we own some.
  PasswordFormData owned_password_data[] = {
      {PasswordForm::SCHEME_HTML, "http://web.site.com/",
       "http://web.site.com/path/to/page.html", NULL, NULL, NULL, NULL,
       L"anonymous", L"knock-knock", false, 0},
      {PasswordForm::SCHEME_BASIC, "http://a.site.com:2222/therealm",
       "http://a.site.com:2222/", NULL, NULL, NULL, NULL, L"username",
       L"password", false, 0},
      {PasswordForm::SCHEME_DIGEST, "https://digest.site.com/differentrealm",
       "https://digest.site.com/secure.html", NULL, NULL, NULL, NULL,
       L"testname", L"testpass", false, 0},
  };
  for (unsigned int i = 0; i < arraysize(owned_password_data); ++i) {
    std::unique_ptr<PasswordForm> form =
        CreatePasswordFormFromDataForTesting(owned_password_data[i]);
    owned_keychain_adapter.AddPassword(*form);
  }

  std::vector<std::unique_ptr<PasswordForm>> all_passwords =
      keychain_adapter.GetAllPasswordFormPasswords();
  EXPECT_EQ(9 + arraysize(owned_password_data), all_passwords.size());

  std::vector<std::unique_ptr<PasswordForm>> owned_passwords =
      owned_keychain_adapter.GetAllPasswordFormPasswords();
  EXPECT_EQ(arraysize(owned_password_data), owned_passwords.size());
}

#pragma mark -

class PasswordStoreMacTest : public testing::Test {
 public:
  PasswordStoreMacTest() : ui_thread_(BrowserThread::UI, &message_loop_) {}

  void SetUp() override {
    ASSERT_TRUE(db_dir_.CreateUniqueTempDir());
    histogram_tester_.reset(new base::HistogramTester);

    // Ensure that LoginDatabase will use the mock keychain if it needs to
    // encrypt/decrypt a password.
    OSCryptMocker::SetUpWithSingleton();
    login_db_.reset(
        new password_manager::LoginDatabase(test_login_db_file_path()));
    thread_.reset(new base::Thread("Chrome_PasswordStore_Thread"));
    ASSERT_TRUE(thread_->Start());
    ASSERT_TRUE(thread_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&PasswordStoreMacTest::InitLoginDatabase,
                              base::Unretained(login_db_.get()))));
    CreateAndInitPasswordStore(login_db_.get());
    // Make sure deferred initialization is performed before some tests start
    // accessing the |login_db| directly.
    FinishAsyncProcessing();
  }

  void TearDown() override {
    ClosePasswordStore();
    thread_.reset();
    login_db_.reset();
    // Whatever a test did, PasswordStoreMac stores only empty password values
    // in LoginDatabase. The empty valus do not require encryption and therefore
    // OSCrypt shouldn't call the Keychain. The histogram doesn't cover the
    // internet passwords.
    if (histogram_tester_) {
      histogram_tester_->ExpectTotalCount("OSX.Keychain.Access", 0);
    }
    OSCryptMocker::TearDown();
  }

  static void InitLoginDatabase(password_manager::LoginDatabase* login_db) {
    ASSERT_TRUE(login_db->Init());
  }

  void CreateAndInitPasswordStore(password_manager::LoginDatabase* login_db) {
    store_ = new PasswordStoreMac(
        base::ThreadTaskRunnerHandle::Get(), nullptr,
        base::WrapUnique<AppleKeychain>(new MockAppleKeychain));
    ASSERT_TRUE(thread_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&PasswordStoreMac::InitWithTaskRunner, store_,
                              thread_->task_runner())));

    ASSERT_TRUE(thread_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&PasswordStoreMac::set_login_metadata_db, store_,
                              base::Unretained(login_db))));
  }

  void ClosePasswordStore() {
    if (!store_)
      return;

    store_->ShutdownOnUIThread();
    store_ = nullptr;
  }

  // Verifies that the given |form| can be properly stored so that it can be
  // retrieved by FillMatchingLogins() and GetAutofillableLogins(), and then it
  // can be properly removed.
  void VerifyCredentialLifecycle(const PasswordForm& form) {
    // Run everything twice to make sure no garbage is left behind that would
    // prevent storing the form a second time.
    for (size_t iteration = 0; iteration < 2; ++iteration) {
      SCOPED_TRACE(testing::Message("Iteration: ") << iteration);

      MockPasswordStoreConsumer mock_consumer;
      EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()))
          .WillOnce(QuitUIMessageLoop());
      store()->GetAutofillableLogins(&mock_consumer);
      base::RunLoop().Run();
      ::testing::Mock::VerifyAndClearExpectations(&mock_consumer);

      store()->AddLogin(form);
      FinishAsyncProcessing();

      PasswordForm returned_form;
      EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(SizeIs(1u)))
          .WillOnce(
              DoAll(SaveACopyOfFirstForm(&returned_form), QuitUIMessageLoop()));

      // The query operations will also do some housekeeping: they will remove
      // dangling credentials in the LoginDatabase without a matching Keychain
      // item when one is expected. If the logic that stores the Keychain item
      // is incorrect, this will wipe the newly added form before the second
      // query.
      store()->GetAutofillableLogins(&mock_consumer);
      base::RunLoop().Run();
      ::testing::Mock::VerifyAndClearExpectations(&mock_consumer);
      EXPECT_EQ(form, returned_form);

      PasswordStore::FormDigest query_form(form);
      EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(SizeIs(1u)))
          .WillOnce(
              DoAll(SaveACopyOfFirstForm(&returned_form), QuitUIMessageLoop()));
      store()->GetLogins(query_form, &mock_consumer);
      base::RunLoop().Run();
      ::testing::Mock::VerifyAndClearExpectations(&mock_consumer);
      EXPECT_EQ(form, returned_form);

      store()->RemoveLogin(form);
    }
  }

  base::FilePath test_login_db_file_path() const {
    return db_dir_.GetPath().Append(FILE_PATH_LITERAL("login.db"));
  }

  password_manager::LoginDatabase* login_db() const {
    return store_->login_metadata_db();
  }

  MockAppleKeychain* keychain() {
    return static_cast<MockAppleKeychain*>(store_->keychain());
  }

  void FinishAsyncProcessing() {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    ASSERT_TRUE(thread_->task_runner()->PostTaskAndReply(
        FROM_HERE, base::Bind(&Noop), runner->QuitClosure()));
    runner->Run();
  }

  PasswordStoreMac* store() { return store_.get(); }

 protected:
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  // Thread that the synchronous methods are run on.
  std::unique_ptr<base::Thread> thread_;

  base::ScopedTempDir db_dir_;
  std::unique_ptr<password_manager::LoginDatabase> login_db_;
  scoped_refptr<PasswordStoreMac> store_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(PasswordStoreMacTest, TestStoreUpdate) {
  // Insert a password into both the database and the keychain.
  // This is done manually, rather than through store_->AddLogin, because the
  // Mock Keychain isn't smart enough to be able to support update generically,
  // so some.domain.com triggers special handling to test it that make inserting
  // fail.
  PasswordFormData joint_data = {PasswordForm::SCHEME_HTML,
                                 "http://some.domain.com/",
                                 "http://some.domain.com/insecure.html",
                                 "login.cgi",
                                 L"username",
                                 L"password",
                                 L"submit",
                                 L"joe_user",
                                 L"sekrit",
                                 true,
                                 1};
  std::unique_ptr<PasswordForm> joint_form =
      CreatePasswordFormFromDataForTesting(joint_data);
  EXPECT_EQ(AddChangeForForm(*joint_form), login_db()->AddLogin(*joint_form));
  MockAppleKeychain::KeychainTestData joint_keychain_data = {
      kSecAuthenticationTypeHTMLForm,
      "some.domain.com",
      kSecProtocolTypeHTTP,
      "/insecure.html",
      0,
      NULL,
      "20020601171500Z",
      "joe_user",
      "sekrit",
      false};
  keychain()->AddTestItem(joint_keychain_data);

  // Insert a password into the keychain only.
  MockAppleKeychain::KeychainTestData keychain_only_data = {
      kSecAuthenticationTypeHTMLForm,
      "keychain.only.com",
      kSecProtocolTypeHTTP,
      NULL,
      0,
      NULL,
      "20020601171500Z",
      "keychain",
      "only",
      false};
  keychain()->AddTestItem(keychain_only_data);

  struct UpdateData {
    PasswordFormData form_data;
    const char* password;  // NULL indicates no entry should be present.
  };

  // Make a series of update calls.
  UpdateData updates[] = {
      // Update the keychain+db passwords (the normal password update case).
      {
          {PasswordForm::SCHEME_HTML, "http://some.domain.com/",
           "http://some.domain.com/insecure.html", "login.cgi", L"username",
           L"password", L"submit", L"joe_user", L"53krit", true, 2},
          "53krit",
      },
      // Update the keychain-only password; this simulates the initial use of a
      // password stored by another browsers.
      {
          {PasswordForm::SCHEME_HTML, "http://keychain.only.com/",
           "http://keychain.only.com/login.html", "login.cgi", L"username",
           L"password", L"submit", L"keychain", L"only", true, 2},
          "only",
      },
      // Update a password that doesn't exist in either location. This tests the
      // case where a form is filled, then the stored login is removed, then the
      // form is submitted.
      {
          {PasswordForm::SCHEME_HTML, "http://different.com/",
           "http://different.com/index.html", "login.cgi", L"username",
           L"password", L"submit", L"abc", L"123", true, 2},
          NULL,
      },
  };
  for (unsigned int i = 0; i < arraysize(updates); ++i) {
    std::unique_ptr<PasswordForm> form =
        CreatePasswordFormFromDataForTesting(updates[i].form_data);
    store_->UpdateLogin(*form);
  }

  FinishAsyncProcessing();

  MacKeychainPasswordFormAdapter keychain_adapter(keychain());
  for (unsigned int i = 0; i < arraysize(updates); ++i) {
    SCOPED_TRACE(testing::Message("iteration ") << i);
    std::unique_ptr<PasswordForm> query_form =
        CreatePasswordFormFromDataForTesting(updates[i].form_data);

    std::vector<std::unique_ptr<PasswordForm>> matching_items =
        keychain_adapter.PasswordsFillingForm(query_form->signon_realm,
                                              query_form->scheme);
    if (updates[i].password) {
      EXPECT_GT(matching_items.size(), 0U);
      if (matching_items.size() >= 1)
        EXPECT_EQ(ASCIIToUTF16(updates[i].password),
                  matching_items[0]->password_value);
    } else {
      EXPECT_EQ(0U, matching_items.size());
    }

    std::vector<std::unique_ptr<PasswordForm>> matching_db_items;
    EXPECT_TRUE(login_db()->GetLogins(PasswordStore::FormDigest(*query_form),
                                      &matching_db_items));
    EXPECT_EQ(updates[i].password ? 1U : 0U, matching_db_items.size());
  }
}

TEST_F(PasswordStoreMacTest, TestDBKeychainAssociation) {
  // Tests that association between the keychain and login database parts of a
  // password added by fuzzy (PSL) matching works.
  // 1. Add a password for www.facebook.com
  // 2. Get a password for m.facebook.com. This fuzzy matches and returns the
  //    www.facebook.com password.
  // 3. Add the returned password for m.facebook.com.
  // 4. Remove both passwords.
  //    -> check: that both are gone from the login DB and the keychain
  // This test should in particular ensure that we don't keep passwords in the
  // keychain just before we think we still have other (fuzzy-)matching entries
  // for them in the login database. (For example, here if we deleted the
  // www.facebook.com password from the login database, we should not be blocked
  // from deleting it from the keystore just becaus the m.facebook.com password
  // fuzzy-matches the www.facebook.com one.)

  // 1. Add a password for www.facebook.com
  PasswordFormData www_form_data = {PasswordForm::SCHEME_HTML,
                                    "http://www.facebook.com/",
                                    "http://www.facebook.com/index.html",
                                    "login",
                                    L"username",
                                    L"password",
                                    L"submit",
                                    L"joe_user",
                                    L"sekrit",
                                    true,
                                    1};
  std::unique_ptr<PasswordForm> www_form =
      CreatePasswordFormFromDataForTesting(www_form_data);
  EXPECT_EQ(AddChangeForForm(*www_form), login_db()->AddLogin(*www_form));
  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain());
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);
  owned_keychain_adapter.AddPassword(*www_form);

  // 2. Get a password for m.facebook.com.
  PasswordForm m_form(*www_form);
  m_form.signon_realm = "http://m.facebook.com";
  m_form.origin = GURL("http://m.facebook.com/index.html");

  MockPasswordStoreConsumer consumer;
  store_->GetLogins(PasswordStore::FormDigest(m_form), &consumer);
  PasswordForm returned_form;
  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(SizeIs(1u)))
      .WillOnce(
          DoAll(SaveACopyOfFirstForm(&returned_form), QuitUIMessageLoop()));
  base::RunLoop().Run();

  // 3. Add the returned password for m.facebook.com.
  returned_form.signon_realm = "http://m.facebook.com";
  returned_form.origin = GURL("http://m.facebook.com/index.html");
  EXPECT_EQ(AddChangeForForm(returned_form),
            login_db()->AddLogin(returned_form));
  owned_keychain_adapter.AddPassword(m_form);

  // 4. Remove both passwords.
  store_->RemoveLogin(*www_form);
  store_->RemoveLogin(m_form);
  FinishAsyncProcessing();

  // No trace of www.facebook.com.
  std::vector<std::unique_ptr<PasswordForm>> matching_items =
      owned_keychain_adapter.PasswordsFillingForm(www_form->signon_realm,
                                                  www_form->scheme);
  EXPECT_EQ(0u, matching_items.size());

  std::vector<std::unique_ptr<PasswordForm>> matching_db_items;
  EXPECT_TRUE(login_db()->GetLogins(PasswordStore::FormDigest(*www_form),
                                    &matching_db_items));
  EXPECT_EQ(0u, matching_db_items.size());

  // No trace of m.facebook.com.
  matching_items = owned_keychain_adapter.PasswordsFillingForm(
      m_form.signon_realm, m_form.scheme);
  EXPECT_EQ(0u, matching_items.size());

  EXPECT_TRUE(login_db()->GetLogins(PasswordStore::FormDigest(m_form),
                                    &matching_db_items));
  EXPECT_EQ(0u, matching_db_items.size());
}

namespace {

class PasswordsChangeObserver
    : public password_manager::PasswordStore::Observer {
 public:
  explicit PasswordsChangeObserver(PasswordStoreMac* store) : observer_(this) {
    observer_.Add(store);
  }

  void WaitAndVerify(PasswordStoreMacTest* test) {
    test->FinishAsyncProcessing();
    ::testing::Mock::VerifyAndClearExpectations(this);
  }

  // password_manager::PasswordStore::Observer:
  MOCK_METHOD1(OnLoginsChanged,
               void(const password_manager::PasswordStoreChangeList& changes));

 private:
  ScopedObserver<password_manager::PasswordStore, PasswordsChangeObserver>
      observer_;
};

password_manager::PasswordStoreChangeList GetAddChangeList(
    const PasswordForm& form) {
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::ADD, form);
  return password_manager::PasswordStoreChangeList(1, change);
}

// Tests RemoveLoginsCreatedBetween or RemoveLoginsSyncedBetween depending on
// |check_created|.
void CheckRemoveLoginsBetween(PasswordStoreMacTest* test, bool check_created) {
  PasswordFormData www_form_data_facebook = {
      PasswordForm::SCHEME_HTML,
      "http://www.facebook.com/",
      "http://www.facebook.com/index.html",
      "login",
      L"submit",
      L"username",
      L"password",
      L"joe_user",
      L"sekrit",
      true,
      0};
  // The old form doesn't have elements names.
  PasswordFormData www_form_data_facebook_old = {
      PasswordForm::SCHEME_HTML,
      "http://www.facebook.com/",
      "http://www.facebook.com/index.html",
      "login",
      L"",
      L"",
      L"",
      L"joe_user",
      L"oldsekrit",
      true,
      0};
  PasswordFormData www_form_data_other = {PasswordForm::SCHEME_HTML,
                                          "http://different.com/",
                                          "http://different.com/index.html",
                                          "login",
                                          L"submit",
                                          L"username",
                                          L"password",
                                          L"different_joe_user",
                                          L"sekrit",
                                          true,
                                          0};
  std::unique_ptr<PasswordForm> form_facebook =
      CreatePasswordFormFromDataForTesting(www_form_data_facebook);
  std::unique_ptr<PasswordForm> form_facebook_old =
      CreatePasswordFormFromDataForTesting(www_form_data_facebook_old);
  std::unique_ptr<PasswordForm> form_other =
      CreatePasswordFormFromDataForTesting(www_form_data_other);
  base::Time now = base::Time::Now();
  base::Time next_day = now + base::TimeDelta::FromDays(1);
  if (check_created) {
    form_facebook_old->date_created = now;
    form_facebook->date_created = next_day;
    form_other->date_created = next_day;
  } else {
    form_facebook_old->date_synced = now;
    form_facebook->date_synced = next_day;
    form_other->date_synced = next_day;
  }

  PasswordsChangeObserver observer(test->store());
  test->store()->AddLogin(*form_facebook_old);
  test->store()->AddLogin(*form_facebook);
  test->store()->AddLogin(*form_other);
  EXPECT_CALL(observer, OnLoginsChanged(GetAddChangeList(*form_facebook_old)));
  EXPECT_CALL(observer, OnLoginsChanged(GetAddChangeList(*form_facebook)));
  EXPECT_CALL(observer, OnLoginsChanged(GetAddChangeList(*form_other)));
  observer.WaitAndVerify(test);

  // Check the keychain content.
  MacKeychainPasswordFormAdapter owned_keychain_adapter(test->keychain());
  owned_keychain_adapter.SetFindsOnlyOwnedItems(false);
  std::vector<std::unique_ptr<PasswordForm>> matching_items(
      owned_keychain_adapter.PasswordsFillingForm(form_facebook->signon_realm,
                                                  form_facebook->scheme));
  EXPECT_EQ(1u, matching_items.size());
  matching_items = owned_keychain_adapter.PasswordsFillingForm(
      form_other->signon_realm, form_other->scheme);
  EXPECT_EQ(1u, matching_items.size());

  // Remove facebook.
  if (check_created) {
    test->store()->RemoveLoginsCreatedBetween(base::Time(), next_day,
                                              base::Closure());
  } else {
    test->store()->RemoveLoginsSyncedBetween(base::Time(), next_day);
  }
  password_manager::PasswordStoreChangeList list;
  form_facebook_old->password_value.clear();
  form_facebook->password_value.clear();
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, *form_facebook_old));
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, *form_facebook));
  EXPECT_CALL(observer, OnLoginsChanged(list));
  list.clear();
  observer.WaitAndVerify(test);

  matching_items = owned_keychain_adapter.PasswordsFillingForm(
      form_facebook->signon_realm, form_facebook->scheme);
  EXPECT_EQ(0u, matching_items.size());
  matching_items = owned_keychain_adapter.PasswordsFillingForm(
      form_other->signon_realm, form_other->scheme);
  EXPECT_EQ(1u, matching_items.size());

  // Remove form_other.
  if (check_created) {
    test->store()->RemoveLoginsCreatedBetween(next_day, base::Time(),
                                              base::Closure());
  } else {
    test->store()->RemoveLoginsSyncedBetween(next_day, base::Time());
  }
  form_other->password_value.clear();
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, *form_other));
  EXPECT_CALL(observer, OnLoginsChanged(list));
  observer.WaitAndVerify(test);
  matching_items = owned_keychain_adapter.PasswordsFillingForm(
      form_other->signon_realm, form_other->scheme);
  EXPECT_EQ(0u, matching_items.size());
}

}  // namespace

TEST_F(PasswordStoreMacTest, TestRemoveLoginsCreatedBetween) {
  CheckRemoveLoginsBetween(this, true);
}

TEST_F(PasswordStoreMacTest, TestRemoveLoginsSyncedBetween) {
  CheckRemoveLoginsBetween(this, false);
}

TEST_F(PasswordStoreMacTest, TestDisableAutoSignInForOrigins) {
  PasswordFormData www_form_data_facebook = {
      PasswordForm::SCHEME_HTML,
      "http://www.facebook.com/",
      "http://www.facebook.com/index.html",
      "login",
      L"submit",
      L"username",
      L"password",
      L"joe_user",
      L"sekrit",
      true,
      0};
  std::unique_ptr<PasswordForm> form_facebook =
      CreatePasswordFormFromDataForTesting(www_form_data_facebook);
  form_facebook->skip_zero_click = false;

  PasswordFormData www_form_data_google = {
      PasswordForm::SCHEME_HTML,
      "http://www.google.com/",
      "http://www.google.com/foo/bar/index.html",
      "login",
      L"submit",
      L"username",
      L"password",
      L"joe_user",
      L"sekrit",
      true,
      0};
  std::unique_ptr<PasswordForm> form_google =
      CreatePasswordFormFromDataForTesting(www_form_data_google);
  form_google->skip_zero_click = false;

  // Add the zero-clickable forms to the database.
  PasswordsChangeObserver observer(store());
  store()->AddLogin(*form_facebook);
  store()->AddLogin(*form_google);
  EXPECT_CALL(observer, OnLoginsChanged(GetAddChangeList(*form_facebook)));
  EXPECT_CALL(observer, OnLoginsChanged(GetAddChangeList(*form_google)));
  observer.WaitAndVerify(this);

  std::vector<std::unique_ptr<PasswordForm>> forms;
  EXPECT_TRUE(login_db()->GetAutoSignInLogins(&forms));
  EXPECT_EQ(2u, forms.size());
  EXPECT_FALSE(forms[0]->skip_zero_click);
  EXPECT_FALSE(forms[1]->skip_zero_click);

  store()->DisableAutoSignInForOrigins(
      base::Bind(static_cast<bool (*)(const GURL&, const GURL&)>(operator==),
                 form_google->origin),
      base::Closure());
  FinishAsyncProcessing();

  EXPECT_TRUE(login_db()->GetAutoSignInLogins(&forms));
  EXPECT_EQ(1u, forms.size());
  EXPECT_EQ(form_facebook->origin, forms[0]->origin);
}

TEST_F(PasswordStoreMacTest, TestRemoveLoginsMultiProfile) {
  // Make sure that RemoveLoginsCreatedBetween does affect only the correct
  // profile.

  // Add a third-party password.
  MockAppleKeychain::KeychainTestData keychain_data = {
      kSecAuthenticationTypeHTMLForm,
      "some.domain.com",
      kSecProtocolTypeHTTP,
      "/insecure.html",
      0,
      NULL,
      "20020601171500Z",
      "joe_user",
      "sekrit",
      false};
  keychain()->AddTestItem(keychain_data);

  // Add a password through the adapter. It has the "Chrome" creator tag.
  // However, it's not referenced by the password database.
  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain());
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);
  PasswordFormData www_form_data1 = {PasswordForm::SCHEME_HTML,
                                     "http://www.facebook.com/",
                                     "http://www.facebook.com/index.html",
                                     "login",
                                     L"username",
                                     L"password",
                                     L"submit",
                                     L"joe_user",
                                     L"sekrit",
                                     true,
                                     1};
  std::unique_ptr<PasswordForm> www_form =
      CreatePasswordFormFromDataForTesting(www_form_data1);
  EXPECT_TRUE(owned_keychain_adapter.AddPassword(*www_form));

  // Add a password from the current profile.
  PasswordFormData www_form_data2 = {PasswordForm::SCHEME_HTML,
                                     "http://www.facebook.com/",
                                     "http://www.facebook.com/index.html",
                                     "login",
                                     L"username",
                                     L"password",
                                     L"submit",
                                     L"not_joe_user",
                                     L"12345",
                                     true,
                                     1};
  www_form = CreatePasswordFormFromDataForTesting(www_form_data2);
  store_->AddLogin(*www_form);
  FinishAsyncProcessing();

  std::vector<std::unique_ptr<PasswordForm>> matching_items;
  EXPECT_TRUE(login_db()->GetLogins(PasswordStore::FormDigest(*www_form),
                                    &matching_items));
  EXPECT_EQ(1u, matching_items.size());

  store_->RemoveLoginsCreatedBetween(base::Time(), base::Time(),
                                     base::Closure());
  FinishAsyncProcessing();

  // Check the second facebook form is gone.
  EXPECT_TRUE(login_db()->GetLogins(PasswordStore::FormDigest(*www_form),
                                    &matching_items));
  EXPECT_EQ(0u, matching_items.size());

  // Check the first facebook form is still there.
  std::vector<std::unique_ptr<PasswordForm>> matching_keychain_items;
  matching_keychain_items = owned_keychain_adapter.PasswordsFillingForm(
      www_form->signon_realm, www_form->scheme);
  ASSERT_EQ(1u, matching_keychain_items.size());
  EXPECT_EQ(ASCIIToUTF16("joe_user"),
            matching_keychain_items[0]->username_value);

  // Check the third-party password is still there.
  owned_keychain_adapter.SetFindsOnlyOwnedItems(false);
  matching_keychain_items = owned_keychain_adapter.PasswordsFillingForm(
      "http://some.domain.com/insecure.html", PasswordForm::SCHEME_HTML);
  ASSERT_EQ(1u, matching_keychain_items.size());
}

// Add a facebook form to the store but not to the keychain. The form is to be
// implicitly deleted. However, the observers shouldn't get notified about
// deletion of non-existent forms like m.facebook.com.
TEST_F(PasswordStoreMacTest, SilentlyRemoveOrphanedForm) {
  testing::StrictMock<password_manager::MockPasswordStoreObserver>
      mock_observer;
  store()->AddObserver(&mock_observer);

  // 1. Add a password for www.facebook.com to the LoginDatabase.
  PasswordFormData www_form_data = {PasswordForm::SCHEME_HTML,
                                    "http://www.facebook.com/",
                                    "http://www.facebook.com/index.html",
                                    "login",
                                    L"username",
                                    L"password",
                                    L"submit",
                                    L"joe_user",
                                    L"",
                                    true,
                                    1};
  std::unique_ptr<PasswordForm> www_form(
      CreatePasswordFormFromDataForTesting(www_form_data));
  EXPECT_EQ(AddChangeForForm(*www_form), login_db()->AddLogin(*www_form));

  // 2. Get a PSL-matched password for m.facebook.com. The observer isn't
  // notified because the form isn't in the database.
  PasswordForm m_form(*www_form);
  m_form.signon_realm = "http://m.facebook.com";
  m_form.origin = GURL("http://m.facebook.com/index.html");

  MockPasswordStoreConsumer consumer;
  ON_CALL(consumer, OnGetPasswordStoreResultsConstRef(_))
      .WillByDefault(QuitUIMessageLoop());
  EXPECT_CALL(mock_observer, OnLoginsChanged(_)).Times(0);
  // The PSL-matched form isn't returned because there is no actual password in
  // the keychain.
  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  store_->GetLogins(PasswordStore::FormDigest(m_form), &consumer);
  base::RunLoop().Run();
  std::vector<std::unique_ptr<PasswordForm>> all_forms;
  EXPECT_TRUE(login_db()->GetAutofillableLogins(&all_forms));
  EXPECT_EQ(1u, all_forms.size());
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // 3. Get a password for www.facebook.com. The form is implicitly removed and
  // the observer is notified.
  password_manager::PasswordStoreChangeList list;
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, *www_form));
  EXPECT_CALL(mock_observer, OnLoginsChanged(list));
  EXPECT_CALL(consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  store_->GetLogins(PasswordStore::FormDigest(*www_form), &consumer);
  base::RunLoop().Run();
  EXPECT_TRUE(login_db()->GetAutofillableLogins(&all_forms));
  EXPECT_EQ(0u, all_forms.size());
}

// Verify that Android app passwords can be stored, retrieved, and deleted.
// Regression test for http://crbug.com/455551
TEST_F(PasswordStoreMacTest, StoringAndRetrievingAndroidCredentials) {
  PasswordForm form;
  form.signon_realm = "android://7x7IDboo8u9YKraUsbmVkuf1@net.rateflix.app/";
  form.username_value = base::UTF8ToUTF16("randomusername");
  form.password_value = base::UTF8ToUTF16("password");

  VerifyCredentialLifecycle(form);
}

// Verify that federated credentials can be stored, retrieved and deleted.
TEST_F(PasswordStoreMacTest, StoringAndRetrievingFederatedCredentials) {
  PasswordForm form;
  form.signon_realm = "android://7x7IDboo8u9YKraUsbmVkuf1@net.rateflix.app/";
  form.federation_origin =
      url::Origin(GURL(password_manager::kTestingFederationUrlSpec));
  form.username_value = base::UTF8ToUTF16("randomusername");
  form.password_value = base::UTF8ToUTF16("");  // No password.

  VerifyCredentialLifecycle(form);
}

void CheckMigrationResult(PasswordStoreMac::MigrationResult expected_result,
                          PasswordStoreMac::MigrationResult result) {
  EXPECT_EQ(expected_result, result);
  QuitUIMessageLoop();
}

// Import the passwords from the Keychain to LoginDatabase.
TEST_F(PasswordStoreMacTest, ImportFromKeychain) {
  PasswordForm form1;
  form1.origin = GURL("http://accounts.google.com/LoginAuth");
  form1.signon_realm = "http://accounts.google.com/";
  form1.username_value = ASCIIToUTF16("my_username");
  form1.password_value = ASCIIToUTF16("my_password");

  PasswordForm form2;
  form2.origin = GURL("http://facebook.com/Login");
  form2.signon_realm = "http://facebook.com/";
  form2.username_value = ASCIIToUTF16("my_username");
  form2.password_value = ASCIIToUTF16("my_password");

  PasswordForm blacklisted_form;
  blacklisted_form.origin = GURL("http://badsite.com/Login");
  blacklisted_form.signon_realm = "http://badsite.com/";
  blacklisted_form.blacklisted_by_user = true;

  store()->AddLogin(form1);
  store()->AddLogin(form2);
  store()->AddLogin(blacklisted_form);
  FinishAsyncProcessing();

  ASSERT_TRUE(base::PostTaskAndReplyWithResult(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&PasswordStoreMac::ImportFromKeychain, login_db(), keychain()),
      base::Bind(&CheckMigrationResult, PasswordStoreMac::MIGRATION_OK)));
  FinishAsyncProcessing();

  // The password should be stored in the database by now.
  std::vector<std::unique_ptr<PasswordForm>> matching_items;
  EXPECT_TRUE(
      login_db()->GetLogins(PasswordStore::FormDigest(form1), &matching_items));
  ASSERT_EQ(1u, matching_items.size());
  EXPECT_EQ(form1, *matching_items[0]);

  EXPECT_TRUE(
      login_db()->GetLogins(PasswordStore::FormDigest(form2), &matching_items));
  ASSERT_EQ(1u, matching_items.size());
  EXPECT_EQ(form2, *matching_items[0]);

  EXPECT_TRUE(login_db()->GetLogins(PasswordStore::FormDigest(blacklisted_form),
                                    &matching_items));
  ASSERT_EQ(1u, matching_items.size());
  EXPECT_EQ(blacklisted_form, *matching_items[0]);

  // The passwords are encrypted using a key from the Keychain.
  EXPECT_TRUE(
      histogram_tester_->GetHistogramSamplesSinceCreation("OSX.Keychain.Access")
          ->TotalCount());
  histogram_tester_.reset();
}

// Import a federated credential while the Keychain is locked.
TEST_F(PasswordStoreMacTest, ImportFederatedFromLockedKeychain) {
  keychain()->set_locked(true);
  PasswordForm form1;
  form1.origin = GURL("http://example.com/Login");
  form1.signon_realm = "http://example.com/";
  form1.username_value = ASCIIToUTF16("my_username");
  form1.federation_origin = url::Origin(GURL("https://accounts.google.com/"));

  store()->AddLogin(form1);
  FinishAsyncProcessing();
  ASSERT_TRUE(base::PostTaskAndReplyWithResult(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&PasswordStoreMac::ImportFromKeychain, login_db(), keychain()),
      base::Bind(&CheckMigrationResult, PasswordStoreMac::MIGRATION_OK)));
  FinishAsyncProcessing();

  std::vector<std::unique_ptr<PasswordForm>> matching_items;
  EXPECT_TRUE(
      login_db()->GetLogins(PasswordStore::FormDigest(form1), &matching_items));
  ASSERT_EQ(1u, matching_items.size());
  EXPECT_EQ(form1, *matching_items[0]);
}

// Try to import while the Keychain is locked but the encryption key had been
// read earlier.
TEST_F(PasswordStoreMacTest, ImportFromLockedKeychainError) {
  PasswordForm form1;
  form1.origin = GURL("http://accounts.google.com/LoginAuth");
  form1.signon_realm = "http://accounts.google.com/";
  form1.username_value = ASCIIToUTF16("my_username");
  form1.password_value = ASCIIToUTF16("my_password");
  store()->AddLogin(form1);
  FinishAsyncProcessing();

  // Add a second keychain item matching the Database entry.
  PasswordForm form2 = form1;
  form2.origin = GURL("http://accounts.google.com/Login");
  form2.password_value = ASCIIToUTF16("1234");
  MacKeychainPasswordFormAdapter adapter(keychain());
  EXPECT_TRUE(adapter.AddPassword(form2));

  keychain()->set_locked(true);
  ASSERT_TRUE(base::PostTaskAndReplyWithResult(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&PasswordStoreMac::ImportFromKeychain, login_db(), keychain()),
      base::Bind(&CheckMigrationResult, PasswordStoreMac::MIGRATION_PARTIAL)));
  FinishAsyncProcessing();

  std::vector<std::unique_ptr<PasswordForm>> matching_items;
  EXPECT_TRUE(
      login_db()->GetLogins(PasswordStore::FormDigest(form1), &matching_items));
  EXPECT_EQ(0u, matching_items.size());

  histogram_tester_->ExpectUniqueSample(
      "PasswordManager.KeychainMigration.NumPasswordsOnFailure", 1, 1);
  histogram_tester_->ExpectUniqueSample(
      "PasswordManager.KeychainMigration.NumFailedPasswords", 1, 1);
  // Don't test the encryption key access.
  histogram_tester_.reset();
}

// Delete the Chrome-owned password from the Keychain.
TEST_F(PasswordStoreMacTest, CleanUpKeychain) {
  MockAppleKeychain::KeychainTestData data1 = { kSecAuthenticationTypeHTMLForm,
     "some.domain.com", kSecProtocolTypeHTTP, NULL, 0, NULL, "20020601171500Z",
     "joe_user", "sekrit", false};
  keychain()->AddTestItem(data1);

  MacKeychainPasswordFormAdapter keychain_adapter(keychain());
  PasswordFormData data2 = { PasswordForm::SCHEME_HTML, "http://web.site.com/",
      "http://web.site.com/path/to/page.html", NULL, NULL, NULL, NULL,
      L"anonymous", L"knock-knock", false, 0 };
  keychain_adapter.AddPassword(*CreatePasswordFormFromDataForTesting(data2));
  std::vector<std::unique_ptr<PasswordForm>> passwords =
      keychain_adapter.GetAllPasswordFormPasswords();
  EXPECT_EQ(2u, passwords.size());

  // Delete everyhting but only the Chrome-owned item should be affected.
  PasswordStoreMac::CleanUpKeychain(keychain(), passwords);
  passwords = keychain_adapter.GetAllPasswordFormPasswords();
  ASSERT_EQ(1u, passwords.size());
  EXPECT_EQ("http://some.domain.com/", passwords[0]->signon_realm);
  EXPECT_EQ(ASCIIToUTF16("sekrit"), passwords[0]->password_value);
}
