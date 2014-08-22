// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_mac.h"

#include "base/basictypes.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_vector.h"
#include "base/path_service.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store_mac_internal.h"
#include "chrome/common/chrome_paths.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "content/public/test/test_browser_thread.h"
#include "crypto/mock_apple_keychain.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using base::WideToUTF16;
using content::BrowserThread;
using crypto::MockAppleKeychain;
using internal_keychain_helpers::FormsMatchForMerge;
using internal_keychain_helpers::STRICT_FORM_MATCH;
using password_manager::LoginDatabase;
using password_manager::PasswordStore;
using password_manager::PasswordStoreConsumer;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::WithArg;

namespace {

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD1(OnGetPasswordStoreResults,
               void(const std::vector<autofill::PasswordForm*>&));

  void CopyElements(const std::vector<autofill::PasswordForm*>& forms) {
    last_result.clear();
    for (size_t i = 0; i < forms.size(); ++i) {
      last_result.push_back(*forms[i]);
    }
  }

  std::vector<PasswordForm> last_result;
};

ACTION(STLDeleteElements0) {
  STLDeleteContainerPointers(arg0.begin(), arg0.end());
}

ACTION(QuitUIMessageLoop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::MessageLoop::current()->Quit();
}

class TestPasswordStoreMac : public PasswordStoreMac {
 public:
  TestPasswordStoreMac(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner,
      crypto::AppleKeychain* keychain,
      LoginDatabase* login_db)
      : PasswordStoreMac(main_thread_runner,
                         db_thread_runner,
                         keychain,
                         login_db) {
  }

  using PasswordStoreMac::GetBackgroundTaskRunner;

 private:
  virtual ~TestPasswordStoreMac() {}

  DISALLOW_COPY_AND_ASSIGN(TestPasswordStoreMac);
};

}  // namespace

#pragma mark -

class PasswordStoreMacInternalsTest : public testing::Test {
 public:
  virtual void SetUp() {
    MockAppleKeychain::KeychainTestData test_data[] = {
      // Basic HTML form.
      { kSecAuthenticationTypeHTMLForm, "some.domain.com",
        kSecProtocolTypeHTTP, NULL, 0, NULL, "20020601171500Z",
        "joe_user", "sekrit", false },
      // HTML form with path.
      { kSecAuthenticationTypeHTMLForm, "some.domain.com",
        kSecProtocolTypeHTTP, "/insecure.html", 0, NULL, "19991231235959Z",
        "joe_user", "sekrit", false },
      // Secure HTML form with path.
      { kSecAuthenticationTypeHTMLForm, "some.domain.com",
        kSecProtocolTypeHTTPS, "/secure.html", 0, NULL, "20100908070605Z",
        "secure_user", "password", false },
      // True negative item.
      { kSecAuthenticationTypeHTMLForm, "dont.remember.com",
        kSecProtocolTypeHTTP, NULL, 0, NULL, "20000101000000Z",
        "", "", true },
      // De-facto negative item, type one.
      { kSecAuthenticationTypeHTMLForm, "dont.remember.com",
        kSecProtocolTypeHTTP, NULL, 0, NULL, "20000101000000Z",
        "Password Not Stored", "", false },
      // De-facto negative item, type two.
      { kSecAuthenticationTypeHTMLForm, "dont.remember.com",
        kSecProtocolTypeHTTPS, NULL, 0, NULL, "20000101000000Z",
        "Password Not Stored", " ", false },
      // HTTP auth basic, with port and path.
      { kSecAuthenticationTypeHTTPBasic, "some.domain.com",
        kSecProtocolTypeHTTP, "/insecure.html", 4567, "low_security",
        "19980330100000Z",
        "basic_auth_user", "basic", false },
      // HTTP auth digest, secure.
      { kSecAuthenticationTypeHTTPDigest, "some.domain.com",
        kSecProtocolTypeHTTPS, NULL, 0, "high_security", "19980330100000Z",
        "digest_auth_user", "digest", false },
      // An FTP password with an invalid date, for edge-case testing.
      { kSecAuthenticationTypeDefault, "a.server.com",
        kSecProtocolTypeFTP, NULL, 0, NULL, "20010203040",
        "abc", "123", false },
    };

    keychain_ = new MockAppleKeychain();

    for (unsigned int i = 0; i < arraysize(test_data); ++i) {
      keychain_->AddTestItem(test_data[i]);
    }
  }

  virtual void TearDown() {
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

// Struct used for creation of PasswordForms from static arrays of data.
struct PasswordFormData {
  const PasswordForm::Scheme scheme;
  const char* signon_realm;
  const char* origin;
  const char* action;
  const wchar_t* submit_element;
  const wchar_t* username_element;
  const wchar_t* password_element;
  const wchar_t* username_value;  // Set to NULL for a blacklist entry.
  const wchar_t* password_value;
  const bool preferred;
  const bool ssl_valid;
  const double creation_time;
};

// Creates and returns a new PasswordForm built from form_data. Caller is
// responsible for deleting the object when finished with it.
static PasswordForm* CreatePasswordFormFromData(
    const PasswordFormData& form_data) {
  PasswordForm* form = new PasswordForm();
  form->scheme = form_data.scheme;
  form->preferred = form_data.preferred;
  form->ssl_valid = form_data.ssl_valid;
  form->date_created = base::Time::FromDoubleT(form_data.creation_time);
  form->date_synced = form->date_created + base::TimeDelta::FromDays(1);
  if (form_data.signon_realm)
    form->signon_realm = std::string(form_data.signon_realm);
  if (form_data.origin)
    form->origin = GURL(form_data.origin);
  if (form_data.action)
    form->action = GURL(form_data.action);
  if (form_data.submit_element)
    form->submit_element = WideToUTF16(form_data.submit_element);
  if (form_data.username_element)
    form->username_element = WideToUTF16(form_data.username_element);
  if (form_data.password_element)
    form->password_element = WideToUTF16(form_data.password_element);
  if (form_data.username_value) {
    form->username_value = WideToUTF16(form_data.username_value);
    form->display_name = form->username_value;
    form->is_zero_click = true;
    if (form_data.password_value)
      form->password_value = WideToUTF16(form_data.password_value);
  } else {
    form->blacklisted_by_user = true;
  }
  form->avatar_url = GURL("https://accounts.google.com/Avatar");
  form->federation_url = GURL("https://accounts.google.com/login");
  return form;
}

// Macro to simplify calling CheckFormsAgainstExpectations with a useful label.
#define CHECK_FORMS(forms, expectations, i) \
    CheckFormsAgainstExpectations(forms, expectations, #forms, i)

// Ensures that the data in |forms| match |expectations|, causing test failures
// for any discrepencies.
// TODO(stuartmorgan): This is current order-dependent; ideally it shouldn't
// matter if |forms| and |expectations| are scrambled.
static void CheckFormsAgainstExpectations(
    const std::vector<PasswordForm*>& forms,
    const std::vector<PasswordFormData*>& expectations,
    const char* forms_label, unsigned int test_number) {
  const unsigned int kBufferSize = 128;
  char test_label[kBufferSize];
  snprintf(test_label, kBufferSize, "%s in test %u", forms_label, test_number);

  EXPECT_EQ(expectations.size(), forms.size()) << test_label;
  if (expectations.size() != forms.size())
    return;

  for (unsigned int i = 0; i < expectations.size(); ++i) {
    snprintf(test_label, kBufferSize, "%s in test %u, item %u",
             forms_label, test_number, i);
    PasswordForm* form = forms[i];
    PasswordFormData* expectation = expectations[i];
    EXPECT_EQ(expectation->scheme, form->scheme) << test_label;
    EXPECT_EQ(std::string(expectation->signon_realm), form->signon_realm)
        << test_label;
    EXPECT_EQ(GURL(expectation->origin), form->origin) << test_label;
    EXPECT_EQ(GURL(expectation->action), form->action) << test_label;
    EXPECT_EQ(WideToUTF16(expectation->submit_element), form->submit_element)
        << test_label;
    EXPECT_EQ(WideToUTF16(expectation->username_element),
              form->username_element) << test_label;
    EXPECT_EQ(WideToUTF16(expectation->password_element),
              form->password_element) << test_label;
    if (expectation->username_value) {
      EXPECT_EQ(WideToUTF16(expectation->username_value),
                form->username_value) << test_label;
      EXPECT_EQ(WideToUTF16(expectation->username_value),
                form->display_name) << test_label;
      EXPECT_TRUE(form->is_zero_click) << test_label;
      EXPECT_EQ(WideToUTF16(expectation->password_value),
                form->password_value) << test_label;
    } else {
      EXPECT_TRUE(form->blacklisted_by_user) << test_label;
    }
    EXPECT_EQ(expectation->preferred, form->preferred)  << test_label;
    EXPECT_EQ(expectation->ssl_valid, form->ssl_valid) << test_label;
    EXPECT_DOUBLE_EQ(expectation->creation_time,
                     form->date_created.ToDoubleT()) << test_label;
    base::Time created = base::Time::FromDoubleT(expectation->creation_time);
    EXPECT_EQ(created + base::TimeDelta::FromDays(1),
              form->date_synced) << test_label;
    EXPECT_EQ(GURL("https://accounts.google.com/Avatar"), form->avatar_url);
    EXPECT_EQ(GURL("https://accounts.google.com/login"), form->federation_url);
  }
}

#pragma mark -

TEST_F(PasswordStoreMacInternalsTest, TestKeychainToFormTranslation) {
  typedef struct {
    const PasswordForm::Scheme scheme;
    const char* signon_realm;
    const char* origin;
    const wchar_t* username;  // Set to NULL to check for a blacklist entry.
    const wchar_t* password;
    const bool ssl_valid;
    const int creation_year;
    const int creation_month;
    const int creation_day;
    const int creation_hour;
    const int creation_minute;
    const int creation_second;
  } TestExpectations;

  TestExpectations expected[] = {
    { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
      "http://some.domain.com/", L"joe_user", L"sekrit", false,
      2002,  6,  1, 17, 15,  0 },
    { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
      "http://some.domain.com/insecure.html", L"joe_user", L"sekrit", false,
      1999, 12, 31, 23, 59, 59 },
    { PasswordForm::SCHEME_HTML, "https://some.domain.com/",
      "https://some.domain.com/secure.html", L"secure_user", L"password", true,
      2010,  9,  8,  7,  6,  5 },
    { PasswordForm::SCHEME_HTML, "http://dont.remember.com/",
      "http://dont.remember.com/", NULL, NULL, false,
      2000,  1,  1,  0,  0,  0 },
    { PasswordForm::SCHEME_HTML, "http://dont.remember.com/",
      "http://dont.remember.com/", NULL, NULL, false,
      2000,  1,  1,  0,  0,  0 },
    { PasswordForm::SCHEME_HTML, "https://dont.remember.com/",
      "https://dont.remember.com/", NULL, NULL, true,
      2000,  1,  1,  0,  0,  0 },
    { PasswordForm::SCHEME_BASIC, "http://some.domain.com:4567/low_security",
      "http://some.domain.com:4567/insecure.html", L"basic_auth_user", L"basic",
      false, 1998, 03, 30, 10, 00, 00 },
    { PasswordForm::SCHEME_DIGEST, "https://some.domain.com/high_security",
      "https://some.domain.com/", L"digest_auth_user", L"digest", true,
      1998,  3, 30, 10,  0,  0 },
    // This one gives us an invalid date, which we will treat as a "NULL" date
    // which is 1601.
    { PasswordForm::SCHEME_OTHER, "http://a.server.com/",
      "http://a.server.com/", L"abc", L"123", false,
      1601,  1,  1,  0,  0,  0 },
  };

  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(expected); ++i) {
    // Create our fake KeychainItemRef; see MockAppleKeychain docs.
    SecKeychainItemRef keychain_item =
        reinterpret_cast<SecKeychainItemRef>(i + 1);
    PasswordForm form;
    bool parsed = internal_keychain_helpers::FillPasswordFormFromKeychainItem(
        *keychain_, keychain_item, &form, true);

    EXPECT_TRUE(parsed) << "In iteration " << i;

    EXPECT_EQ(expected[i].scheme, form.scheme) << "In iteration " << i;
    EXPECT_EQ(GURL(expected[i].origin), form.origin) << "In iteration " << i;
    EXPECT_EQ(expected[i].ssl_valid, form.ssl_valid) << "In iteration " << i;
    EXPECT_EQ(std::string(expected[i].signon_realm), form.signon_realm)
        << "In iteration " << i;
    if (expected[i].username) {
      EXPECT_EQ(WideToUTF16(expected[i].username), form.username_value)
          << "In iteration " << i;
      EXPECT_EQ(WideToUTF16(expected[i].password), form.password_value)
          << "In iteration " << i;
      EXPECT_FALSE(form.blacklisted_by_user) << "In iteration " << i;
    } else {
      EXPECT_TRUE(form.blacklisted_by_user) << "In iteration " << i;
    }
    base::Time::Exploded exploded_time;
    form.date_created.UTCExplode(&exploded_time);
    EXPECT_EQ(expected[i].creation_year, exploded_time.year)
         << "In iteration " << i;
    EXPECT_EQ(expected[i].creation_month, exploded_time.month)
        << "In iteration " << i;
    EXPECT_EQ(expected[i].creation_day, exploded_time.day_of_month)
        << "In iteration " << i;
    EXPECT_EQ(expected[i].creation_hour, exploded_time.hour)
        << "In iteration " << i;
    EXPECT_EQ(expected[i].creation_minute, exploded_time.minute)
        << "In iteration " << i;
    EXPECT_EQ(expected[i].creation_second, exploded_time.second)
        << "In iteration " << i;
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
    { { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        NULL, NULL, NULL, NULL, NULL, L"joe_user", NULL, false, false, 0 },
      2, 2 },
    { { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        NULL, NULL, NULL, NULL, NULL, L"wrong_user", NULL, false, false, 0 },
      2, 0 },
    // An HTML form we haven't seen
    { { PasswordForm::SCHEME_HTML, "http://www.unseendomain.com/",
        NULL, NULL, NULL, NULL, NULL, L"joe_user", NULL, false, false, 0 },
      0, 0 },
    // Basic auth that should match.
    { { PasswordForm::SCHEME_BASIC, "http://some.domain.com:4567/low_security",
        NULL, NULL, NULL, NULL, NULL, L"basic_auth_user", NULL, false, false,
        0 },
      1, 1 },
    // Basic auth with the wrong port.
    { { PasswordForm::SCHEME_BASIC, "http://some.domain.com:1111/low_security",
        NULL, NULL, NULL, NULL, NULL, L"basic_auth_user", NULL, false, false,
        0 },
      0, 0 },
    // Digest auth we've saved under https, visited with http.
    { { PasswordForm::SCHEME_DIGEST, "http://some.domain.com/high_security",
        NULL, NULL, NULL, NULL, NULL, L"digest_auth_user", NULL, false, false,
        0 },
      0, 0 },
    // Digest auth that should match.
    { { PasswordForm::SCHEME_DIGEST, "https://some.domain.com/high_security",
        NULL, NULL, NULL, NULL, NULL, L"wrong_user", NULL, false, true, 0 },
      1, 0 },
    // Digest auth with the wrong domain.
    { { PasswordForm::SCHEME_DIGEST, "https://some.domain.com/other_domain",
        NULL, NULL, NULL, NULL, NULL, L"digest_auth_user", NULL, false, true,
        0 },
      0, 0 },
    // Garbage forms should have no matches.
    { { PasswordForm::SCHEME_HTML, "foo/bar/baz",
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, false, false, 0 }, 0, 0 },
  };

  MacKeychainPasswordFormAdapter keychain_adapter(keychain_);
  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_);
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    scoped_ptr<PasswordForm> query_form(
        CreatePasswordFormFromData(test_data[i].data));

    // Check matches treating the form as a fill target.
    std::vector<PasswordForm*> matching_items =
        keychain_adapter.PasswordsFillingForm(query_form->signon_realm,
                                              query_form->scheme);
    EXPECT_EQ(test_data[i].expected_fill_matches, matching_items.size());
    STLDeleteElements(&matching_items);

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
    STLDeleteContainerPairSecondPointers(item_form_pairs.begin(),
                                         item_form_pairs.end());
    for (std::vector<SecKeychainItemRef>::iterator i = keychain_items.begin();
         i != keychain_items.end(); ++i) {
      keychain_->Free(*i);
    }
    STLDeleteElements(&matching_items);

    // None of the pre-seeded items are owned by us, so none should match an
    // owned-passwords-only search.
    matching_items = owned_keychain_adapter.PasswordsFillingForm(
        query_form->signon_realm, query_form->scheme);
    EXPECT_EQ(0U, matching_items.size());
    STLDeleteElements(&matching_items);
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
    { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
      "http://some.domain.com/insecure.html",
      NULL, NULL, NULL, NULL, L"joe_user", NULL, true, false, 0 },
    { PasswordForm::SCHEME_BASIC, "http://some.domain.com:4567/low_security",
      "http://some.domain.com:4567/insecure.html",
      NULL, NULL, NULL, NULL, L"basic_auth_user", NULL, true, false, 0 },
    { PasswordForm::SCHEME_DIGEST, "https://some.domain.com/high_security",
      "https://some.domain.com",
      NULL, NULL, NULL, NULL, L"digest_auth_user", NULL, true, true, 0 },
  };

  for (unsigned int i = 0; i < arraysize(base_form_data); ++i) {
    // Create a base form and make sure we find a match.
    scoped_ptr<PasswordForm> base_form(CreatePasswordFormFromData(
        base_form_data[i]));
    EXPECT_TRUE(keychain_adapter.HasPasswordsMergeableWithForm(*base_form));
    EXPECT_TRUE(keychain_adapter.HasPasswordExactlyMatchingForm(*base_form));

    // Make sure that the matching isn't looser than it should be by checking
    // that slightly altered forms don't match.
    std::vector<PasswordForm*> modified_forms;

    modified_forms.push_back(new PasswordForm(*base_form));
    modified_forms.back()->username_value = ASCIIToUTF16("wrong_user");

    modified_forms.push_back(new PasswordForm(*base_form));
    SetPasswordFormPath(modified_forms.back(), "elsewhere.html");

    modified_forms.push_back(new PasswordForm(*base_form));
    modified_forms.back()->scheme = PasswordForm::SCHEME_OTHER;

    modified_forms.push_back(new PasswordForm(*base_form));
    SetPasswordFormPort(modified_forms.back(), "1234");

    modified_forms.push_back(new PasswordForm(*base_form));
    modified_forms.back()->blacklisted_by_user = true;

    if (base_form->scheme == PasswordForm::SCHEME_BASIC ||
        base_form->scheme == PasswordForm::SCHEME_DIGEST) {
      modified_forms.push_back(new PasswordForm(*base_form));
      SetPasswordFormRealm(modified_forms.back(), "incorrect");
    }

    for (unsigned int j = 0; j < modified_forms.size(); ++j) {
      bool match = keychain_adapter.HasPasswordExactlyMatchingForm(
          *modified_forms[j]);
      EXPECT_FALSE(match) << "In modified version " << j
          << " of base form " << i;
    }
    STLDeleteElements(&modified_forms);
  }
}

TEST_F(PasswordStoreMacInternalsTest, TestKeychainAdd) {
  struct TestDataAndExpectation {
    PasswordFormData data;
    bool should_succeed;
  };
  TestDataAndExpectation test_data[] = {
    // Test a variety of scheme/port/protocol/path variations.
    { { PasswordForm::SCHEME_HTML, "http://web.site.com/",
        "http://web.site.com/path/to/page.html", NULL, NULL, NULL, NULL,
        L"anonymous", L"knock-knock", false, false, 0 }, true },
    { { PasswordForm::SCHEME_HTML, "https://web.site.com/",
        "https://web.site.com/", NULL, NULL, NULL, NULL,
        L"admin", L"p4ssw0rd", false, false, 0 }, true },
    { { PasswordForm::SCHEME_BASIC, "http://a.site.com:2222/therealm",
        "http://a.site.com:2222/", NULL, NULL, NULL, NULL,
        L"username", L"password", false, false, 0 }, true },
    { { PasswordForm::SCHEME_DIGEST, "https://digest.site.com/differentrealm",
        "https://digest.site.com/secure.html", NULL, NULL, NULL, NULL,
        L"testname", L"testpass", false, false, 0 }, true },
    // Make sure that garbage forms are rejected.
    { { PasswordForm::SCHEME_HTML, "gobbledygook",
        "gobbledygook", NULL, NULL, NULL, NULL,
        L"anonymous", L"knock-knock", false, false, 0 }, false },
    // Test that failing to update a duplicate (forced using the magic failure
    // password; see MockAppleKeychain::ItemModifyAttributesAndData) is
    // reported.
    { { PasswordForm::SCHEME_HTML, "http://some.domain.com",
        "http://some.domain.com/insecure.html", NULL, NULL, NULL, NULL,
        L"joe_user", L"fail_me", false, false, 0 }, false },
  };

  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_);
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);

  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    scoped_ptr<PasswordForm> in_form(
        CreatePasswordFormFromData(test_data[i].data));
    bool add_succeeded = owned_keychain_adapter.AddPassword(*in_form);
    EXPECT_EQ(test_data[i].should_succeed, add_succeeded);
    if (add_succeeded) {
      EXPECT_TRUE(owned_keychain_adapter.HasPasswordsMergeableWithForm(
          *in_form));
      EXPECT_TRUE(owned_keychain_adapter.HasPasswordExactlyMatchingForm(
          *in_form));
    }
  }

  // Test that adding duplicate item updates the existing item.
  {
    PasswordFormData data = {
      PasswordForm::SCHEME_HTML, "http://some.domain.com",
      "http://some.domain.com/insecure.html", NULL,
      NULL, NULL, NULL, L"joe_user", L"updated_password", false, false, 0
    };
    scoped_ptr<PasswordForm> update_form(CreatePasswordFormFromData(data));
    MacKeychainPasswordFormAdapter keychain_adapter(keychain_);
    EXPECT_TRUE(keychain_adapter.AddPassword(*update_form));
    SecKeychainItemRef keychain_item = reinterpret_cast<SecKeychainItemRef>(2);
    PasswordForm stored_form;
    internal_keychain_helpers::FillPasswordFormFromKeychainItem(*keychain_,
                                                                keychain_item,
                                                                &stored_form,
                                                                true);
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
    { { PasswordForm::SCHEME_HTML, "http://web.site.com/",
        "http://web.site.com/path/to/page.html", NULL, NULL, NULL, NULL,
        L"anonymous", L"knock-knock", false, false, 0 }, true },
    // Make sure we don't delete items we don't own.
    { { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/insecure.html", NULL, NULL, NULL, NULL,
        L"joe_user", NULL, true, false, 0 }, false },
  };

  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_);
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);

  // Add our test item so that we can delete it.
  PasswordForm* add_form = CreatePasswordFormFromData(test_data[0].data);
  EXPECT_TRUE(owned_keychain_adapter.AddPassword(*add_form));
  delete add_form;

  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    scoped_ptr<PasswordForm> form(CreatePasswordFormFromData(
        test_data[i].data));
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
    different_form.ssl_valid = true;
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
}

TEST_F(PasswordStoreMacInternalsTest, TestFormMerge) {
  // Set up a bunch of test data to use in varying combinations.
  PasswordFormData keychain_user_1 =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/", "", L"", L"", L"", L"joe_user", L"sekrit",
        false, false, 1010101010 };
  PasswordFormData keychain_user_1_with_path =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/page.html",
        "", L"", L"", L"", L"joe_user", L"otherpassword",
        false, false, 1010101010 };
  PasswordFormData keychain_user_2 =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/", "", L"", L"", L"", L"john.doe", L"sesame",
        false, false, 958739876 };
  PasswordFormData keychain_blacklist =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/", "", L"", L"", L"", NULL, NULL,
        false, false, 1010101010 };

  PasswordFormData db_user_1 =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/", "http://some.domain.com/action.cgi",
        L"submit", L"username", L"password", L"joe_user", L"",
        true, false, 1212121212 };
  PasswordFormData db_user_1_with_path =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/page.html",
        "http://some.domain.com/handlepage.cgi",
        L"submit", L"username", L"password", L"joe_user", L"",
        true, false, 1234567890 };
  PasswordFormData db_user_3_with_path =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/page.html",
        "http://some.domain.com/handlepage.cgi",
        L"submit", L"username", L"password", L"second-account", L"",
        true, false, 1240000000 };
  PasswordFormData database_blacklist_with_path =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/path.html", "http://some.domain.com/action.cgi",
        L"submit", L"username", L"password", NULL, NULL,
        true, false, 1212121212 };

  PasswordFormData merged_user_1 =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/", "http://some.domain.com/action.cgi",
        L"submit", L"username", L"password", L"joe_user", L"sekrit",
        true, false, 1212121212 };
  PasswordFormData merged_user_1_with_db_path =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/page.html",
        "http://some.domain.com/handlepage.cgi",
        L"submit", L"username", L"password", L"joe_user", L"sekrit",
        true, false, 1234567890 };
  PasswordFormData merged_user_1_with_both_paths =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/page.html",
        "http://some.domain.com/handlepage.cgi",
        L"submit", L"username", L"password", L"joe_user", L"otherpassword",
        true, false, 1234567890 };

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
  const unsigned int kTestCount = 4;
  std::vector< std::vector< std::vector<PasswordFormData*> > > test_data(
      MERGE_IO_ARRAY_COUNT, std::vector< std::vector<PasswordFormData*> >(
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

  for (unsigned int test_case = 0; test_case <= current_test; ++test_case) {
    std::vector<PasswordForm*> keychain_forms;
    for (std::vector<PasswordFormData*>::iterator i =
             test_data[KEYCHAIN_INPUT][test_case].begin();
         i != test_data[KEYCHAIN_INPUT][test_case].end(); ++i) {
      keychain_forms.push_back(CreatePasswordFormFromData(*(*i)));
    }
    std::vector<PasswordForm*> database_forms;
    for (std::vector<PasswordFormData*>::iterator i =
             test_data[DATABASE_INPUT][test_case].begin();
         i != test_data[DATABASE_INPUT][test_case].end(); ++i) {
      database_forms.push_back(CreatePasswordFormFromData(*(*i)));
    }

    std::vector<PasswordForm*> merged_forms;
    internal_keychain_helpers::MergePasswordForms(&keychain_forms,
                                                  &database_forms,
                                                  &merged_forms);

    CHECK_FORMS(keychain_forms, test_data[KEYCHAIN_OUTPUT][test_case],
                test_case);
    CHECK_FORMS(database_forms, test_data[DATABASE_OUTPUT][test_case],
                test_case);
    CHECK_FORMS(merged_forms, test_data[MERGE_OUTPUT][test_case], test_case);

    STLDeleteElements(&keychain_forms);
    STLDeleteElements(&database_forms);
    STLDeleteElements(&merged_forms);
  }
}

TEST_F(PasswordStoreMacInternalsTest, TestPasswordBulkLookup) {
  PasswordFormData db_data[] = {
    { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
      "http://some.domain.com/", "http://some.domain.com/action.cgi",
      L"submit", L"username", L"password", L"joe_user", L"",
      true, false, 1212121212 },
    { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
      "http://some.domain.com/page.html",
      "http://some.domain.com/handlepage.cgi",
      L"submit", L"username", L"password", L"joe_user", L"",
      true, false, 1234567890 },
    { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
      "http://some.domain.com/page.html",
      "http://some.domain.com/handlepage.cgi",
      L"submit", L"username", L"password", L"second-account", L"",
      true, false, 1240000000 },
    { PasswordForm::SCHEME_HTML, "http://dont.remember.com/",
      "http://dont.remember.com/",
      "http://dont.remember.com/handlepage.cgi",
      L"submit", L"username", L"password", L"joe_user", L"",
      true, false, 1240000000 },
    { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
      "http://some.domain.com/path.html", "http://some.domain.com/action.cgi",
      L"submit", L"username", L"password", NULL, NULL,
      true, false, 1212121212 },
  };
  std::vector<PasswordForm*> database_forms;
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(db_data); ++i) {
    database_forms.push_back(CreatePasswordFormFromData(db_data[i]));
  }
  std::vector<PasswordForm*> merged_forms =
      internal_keychain_helpers::GetPasswordsForForms(*keychain_,
                                                      &database_forms);
  EXPECT_EQ(2U, database_forms.size());
  ASSERT_EQ(3U, merged_forms.size());
  EXPECT_EQ(ASCIIToUTF16("sekrit"), merged_forms[0]->password_value);
  EXPECT_EQ(ASCIIToUTF16("sekrit"), merged_forms[1]->password_value);
  EXPECT_TRUE(merged_forms[2]->blacklisted_by_user);

  STLDeleteElements(&database_forms);
  STLDeleteElements(&merged_forms);
}

TEST_F(PasswordStoreMacInternalsTest, TestBlacklistedFiltering) {
  PasswordFormData db_data[] = {
    { PasswordForm::SCHEME_HTML, "http://dont.remember.com/",
      "http://dont.remember.com/",
      "http://dont.remember.com/handlepage.cgi",
      L"submit", L"username", L"password", L"joe_user", L"non_empty_password",
      true, false, 1240000000 },
    { PasswordForm::SCHEME_HTML, "https://dont.remember.com/",
      "https://dont.remember.com/",
      "https://dont.remember.com/handlepage_secure.cgi",
      L"submit", L"username", L"password", L"joe_user", L"non_empty_password",
      true, false, 1240000000 },
  };
  std::vector<PasswordForm*> database_forms;
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(db_data); ++i) {
    database_forms.push_back(CreatePasswordFormFromData(db_data[i]));
  }
  std::vector<PasswordForm*> merged_forms =
      internal_keychain_helpers::GetPasswordsForForms(*keychain_,
                                                      &database_forms);
  EXPECT_EQ(2U, database_forms.size());
  ASSERT_EQ(0U, merged_forms.size());

  STLDeleteElements(&database_forms);
  STLDeleteElements(&merged_forms);
}

TEST_F(PasswordStoreMacInternalsTest, TestFillPasswordFormFromKeychainItem) {
  // When |extract_password_data| is false, the password field must be empty,
  // and |blacklisted_by_user| must be false.
  SecKeychainItemRef keychain_item = reinterpret_cast<SecKeychainItemRef>(1);
  PasswordForm form_without_extracted_password;
  bool parsed = internal_keychain_helpers::FillPasswordFormFromKeychainItem(
      *keychain_,
      keychain_item,
      &form_without_extracted_password,
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
      *keychain_,
      keychain_item,
      &form_with_extracted_password,
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
      *keychain_,
      keychain_item,
      &negative_form,
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
      *keychain_,
      keychain_item,
      &form_with_empty_password_a,
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
      *keychain_,
      keychain_item,
      &form_with_empty_password_b,
      true);  // Extract password.
  EXPECT_TRUE(parsed);
  ASSERT_EQ(ASCIIToUTF16(" "),
            form_with_empty_password_b.password_value);
  ASSERT_TRUE(form_with_empty_password_b.blacklisted_by_user);
}

TEST_F(PasswordStoreMacInternalsTest, TestPasswordGetAll) {
  MacKeychainPasswordFormAdapter keychain_adapter(keychain_);
  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_);
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);

  // Add a few passwords of various types so that we own some.
  PasswordFormData owned_password_data[] = {
    { PasswordForm::SCHEME_HTML, "http://web.site.com/",
      "http://web.site.com/path/to/page.html", NULL, NULL, NULL, NULL,
      L"anonymous", L"knock-knock", false, false, 0 },
    { PasswordForm::SCHEME_BASIC, "http://a.site.com:2222/therealm",
      "http://a.site.com:2222/", NULL, NULL, NULL, NULL,
      L"username", L"password", false, false, 0 },
    { PasswordForm::SCHEME_DIGEST, "https://digest.site.com/differentrealm",
      "https://digest.site.com/secure.html", NULL, NULL, NULL, NULL,
      L"testname", L"testpass", false, false, 0 },
  };
  for (unsigned int i = 0; i < arraysize(owned_password_data); ++i) {
    scoped_ptr<PasswordForm> form(CreatePasswordFormFromData(
        owned_password_data[i]));
    owned_keychain_adapter.AddPassword(*form);
  }

  std::vector<PasswordForm*> all_passwords =
      keychain_adapter.GetAllPasswordFormPasswords();
  EXPECT_EQ(8 + arraysize(owned_password_data), all_passwords.size());
  STLDeleteElements(&all_passwords);

  std::vector<PasswordForm*> owned_passwords =
      owned_keychain_adapter.GetAllPasswordFormPasswords();
  EXPECT_EQ(arraysize(owned_password_data), owned_passwords.size());
  STLDeleteElements(&owned_passwords);
}

#pragma mark -

class PasswordStoreMacTest : public testing::Test {
 public:
  PasswordStoreMacTest() : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual void SetUp() {
    login_db_ = new LoginDatabase();
    ASSERT_TRUE(db_dir_.CreateUniqueTempDir());
    base::FilePath db_file = db_dir_.path().AppendASCII("login.db");
    ASSERT_TRUE(login_db_->Init(db_file));

    keychain_ = new MockAppleKeychain();

    store_ = new TestPasswordStoreMac(
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current(),
        keychain_,
        login_db_);
    ASSERT_TRUE(store_->Init(syncer::SyncableService::StartSyncFlare(), ""));
  }

  virtual void TearDown() {
    store_->Shutdown();
    EXPECT_FALSE(store_->GetBackgroundTaskRunner());
  }

  void WaitForStoreUpdate() {
    // Do a store-level query to wait for all the operations above to be done.
    MockPasswordStoreConsumer consumer;
    EXPECT_CALL(consumer, OnGetPasswordStoreResults(_))
        .WillOnce(DoAll(WithArg<0>(STLDeleteElements0()), QuitUIMessageLoop()));
    store_->GetLogins(PasswordForm(), PasswordStore::ALLOW_PROMPT, &consumer);
    base::MessageLoop::current()->Run();
  }

  scoped_refptr<TestPasswordStoreMac> store() { return store_; }

  MockAppleKeychain* keychain() { return keychain_; }

 protected:
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  MockAppleKeychain* keychain_;  // Owned by store_.
  LoginDatabase* login_db_;  // Owned by store_.
  scoped_refptr<TestPasswordStoreMac> store_;
  base::ScopedTempDir db_dir_;
};

TEST_F(PasswordStoreMacTest, TestStoreUpdate) {
  // Insert a password into both the database and the keychain.
  // This is done manually, rather than through store_->AddLogin, because the
  // Mock Keychain isn't smart enough to be able to support update generically,
  // so some.domain.com triggers special handling to test it that make inserting
  // fail.
  PasswordFormData joint_data = {
    PasswordForm::SCHEME_HTML, "http://some.domain.com/",
    "http://some.domain.com/insecure.html", "login.cgi",
    L"username", L"password", L"submit", L"joe_user", L"sekrit", true, false, 1
  };
  scoped_ptr<PasswordForm> joint_form(CreatePasswordFormFromData(joint_data));
  login_db_->AddLogin(*joint_form);
  MockAppleKeychain::KeychainTestData joint_keychain_data = {
    kSecAuthenticationTypeHTMLForm, "some.domain.com",
    kSecProtocolTypeHTTP, "/insecure.html", 0, NULL, "20020601171500Z",
    "joe_user", "sekrit", false };
  keychain_->AddTestItem(joint_keychain_data);

  // Insert a password into the keychain only.
  MockAppleKeychain::KeychainTestData keychain_only_data = {
    kSecAuthenticationTypeHTMLForm, "keychain.only.com",
    kSecProtocolTypeHTTP, NULL, 0, NULL, "20020601171500Z",
    "keychain", "only", false
  };
  keychain_->AddTestItem(keychain_only_data);

  struct UpdateData {
    PasswordFormData form_data;
    const char* password;  // NULL indicates no entry should be present.
  };

  // Make a series of update calls.
  UpdateData updates[] = {
    // Update the keychain+db passwords (the normal password update case).
    { { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/insecure.html", "login.cgi",
        L"username", L"password", L"submit", L"joe_user", L"53krit",
        true, false, 2 },
      "53krit",
    },
    // Update the keychain-only password; this simulates the initial use of a
    // password stored by another browsers.
    { { PasswordForm::SCHEME_HTML, "http://keychain.only.com/",
        "http://keychain.only.com/login.html", "login.cgi",
        L"username", L"password", L"submit", L"keychain", L"only",
        true, false, 2 },
      "only",
    },
    // Update a password that doesn't exist in either location. This tests the
    // case where a form is filled, then the stored login is removed, then the
    // form is submitted.
    { { PasswordForm::SCHEME_HTML, "http://different.com/",
        "http://different.com/index.html", "login.cgi",
        L"username", L"password", L"submit", L"abc", L"123",
        true, false, 2 },
      NULL,
    },
  };
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(updates); ++i) {
    scoped_ptr<PasswordForm> form(CreatePasswordFormFromData(
        updates[i].form_data));
    store_->UpdateLogin(*form);
  }

  WaitForStoreUpdate();

  MacKeychainPasswordFormAdapter keychain_adapter(keychain_);
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(updates); ++i) {
    scoped_ptr<PasswordForm> query_form(
        CreatePasswordFormFromData(updates[i].form_data));

    std::vector<PasswordForm*> matching_items =
        keychain_adapter.PasswordsFillingForm(query_form->signon_realm,
                                              query_form->scheme);
    if (updates[i].password) {
      EXPECT_GT(matching_items.size(), 0U) << "iteration " << i;
      if (matching_items.size() >= 1)
        EXPECT_EQ(ASCIIToUTF16(updates[i].password),
                  matching_items[0]->password_value) << "iteration " << i;
    } else {
      EXPECT_EQ(0U, matching_items.size()) << "iteration " << i;
    }
    STLDeleteElements(&matching_items);

    login_db_->GetLogins(*query_form, &matching_items);
    EXPECT_EQ(updates[i].password ? 1U : 0U, matching_items.size())
        << "iteration " << i;
    STLDeleteElements(&matching_items);
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
  PasswordFormData www_form_data = {
    PasswordForm::SCHEME_HTML, "http://www.facebook.com/",
    "http://www.facebook.com/index.html", "login",
    L"username", L"password", L"submit", L"joe_user", L"sekrit", true, false, 1
  };
  scoped_ptr<PasswordForm> www_form(CreatePasswordFormFromData(www_form_data));
  login_db_->AddLogin(*www_form);
  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_);
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);
  owned_keychain_adapter.AddPassword(*www_form);

  // 2. Get a password for m.facebook.com.
  PasswordForm m_form(*www_form);
  m_form.signon_realm = "http://m.facebook.com";
  m_form.origin = GURL("http://m.facebook.com/index.html");
  MockPasswordStoreConsumer consumer;
  EXPECT_CALL(consumer, OnGetPasswordStoreResults(_)).WillOnce(DoAll(
      WithArg<0>(Invoke(&consumer, &MockPasswordStoreConsumer::CopyElements)),
      WithArg<0>(STLDeleteElements0()),
      QuitUIMessageLoop()));
  store_->GetLogins(m_form, PasswordStore::ALLOW_PROMPT, &consumer);
  base::MessageLoop::current()->Run();
  EXPECT_EQ(1u, consumer.last_result.size());

  // 3. Add the returned password for m.facebook.com.
  login_db_->AddLogin(consumer.last_result[0]);
  owned_keychain_adapter.AddPassword(m_form);

  // 4. Remove both passwords.
  store_->RemoveLogin(*www_form);
  store_->RemoveLogin(m_form);
  WaitForStoreUpdate();

  std::vector<PasswordForm*> matching_items;
  // No trace of www.facebook.com.
  matching_items = owned_keychain_adapter.PasswordsFillingForm(
      www_form->signon_realm, www_form->scheme);
  EXPECT_EQ(0u, matching_items.size());
  login_db_->GetLogins(*www_form, &matching_items);
  EXPECT_EQ(0u, matching_items.size());
  // No trace of m.facebook.com.
  matching_items = owned_keychain_adapter.PasswordsFillingForm(
      m_form.signon_realm, m_form.scheme);
  EXPECT_EQ(0u, matching_items.size());
  login_db_->GetLogins(m_form, &matching_items);
  EXPECT_EQ(0u, matching_items.size());
}

namespace {

class PasswordsChangeObserver :
    public password_manager::PasswordStore::Observer {
public:
  PasswordsChangeObserver(TestPasswordStoreMac* store) : observer_(this) {
    observer_.Add(store);
  }

  void WaitAndVerify(PasswordStoreMacTest* test) {
    test->WaitForStoreUpdate();
    ::testing::Mock::VerifyAndClearExpectations(this);
  }

  // password_manager::PasswordStore::Observer:
  MOCK_METHOD1(OnLoginsChanged,
               void(const password_manager::PasswordStoreChangeList& changes));

private:
  ScopedObserver<password_manager::PasswordStore,
                PasswordsChangeObserver> observer_;
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
      PasswordForm::SCHEME_HTML, "http://www.facebook.com/",
      "http://www.facebook.com/index.html", "login", L"submit", L"username",
      L"password", L"joe_user", L"sekrit", true, false, 0 };
  // The old form doesn't have elements names.
  PasswordFormData www_form_data_facebook_old = {
      PasswordForm::SCHEME_HTML, "http://www.facebook.com/",
      "http://www.facebook.com/index.html", "login", L"", L"",
      L"", L"joe_user", L"oldsekrit", true, false, 0 };
  PasswordFormData www_form_data_other = {
      PasswordForm::SCHEME_HTML, "http://different.com/",
      "http://different.com/index.html", "login", L"submit", L"username",
      L"password", L"different_joe_user", L"sekrit", true, false, 0 };
  scoped_ptr<PasswordForm> form_facebook(
      CreatePasswordFormFromData(www_form_data_facebook));
  scoped_ptr<PasswordForm> form_facebook_old(
      CreatePasswordFormFromData(www_form_data_facebook_old));
  scoped_ptr<PasswordForm> form_other(
      CreatePasswordFormFromData(www_form_data_other));
  base::Time now = base::Time::Now();
  // TODO(vasilii): remove the next line once crbug/374132 is fixed.
  now = base::Time::FromTimeT(now.ToTimeT());
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
  ScopedVector<PasswordForm> matching_items;
  matching_items.get() = owned_keychain_adapter.PasswordsFillingForm(
      form_facebook->signon_realm, form_facebook->scheme);
  EXPECT_EQ(1u, matching_items.size());
  matching_items.clear();
  matching_items.get() = owned_keychain_adapter.PasswordsFillingForm(
      form_other->signon_realm, form_other->scheme);
  EXPECT_EQ(1u, matching_items.size());
  matching_items.clear();

  // Remove facebook.
  void (PasswordStore::*method)(base::Time, base::Time) =
      check_created ? &PasswordStore::RemoveLoginsCreatedBetween
                    : &PasswordStore::RemoveLoginsSyncedBetween;
  (test->store()->*method)(base::Time(), next_day);
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

  matching_items.get() = owned_keychain_adapter.PasswordsFillingForm(
      form_facebook->signon_realm, form_facebook->scheme);
  EXPECT_EQ(0u, matching_items.size());
  matching_items.get() = owned_keychain_adapter.PasswordsFillingForm(
      form_other->signon_realm, form_other->scheme);
  EXPECT_EQ(1u, matching_items.size());
  matching_items.clear();

  // Remove form_other.
  (test->store()->*method)(next_day, base::Time());
  form_other->password_value.clear();
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, *form_other));
  EXPECT_CALL(observer, OnLoginsChanged(list));
  observer.WaitAndVerify(test);
  matching_items.get() = owned_keychain_adapter.PasswordsFillingForm(
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

TEST_F(PasswordStoreMacTest, TestRemoveLoginsMultiProfile) {
  // Make sure that RemoveLoginsCreatedBetween does affect only the correct
  // profile.

  // Add a third-party password.
  MockAppleKeychain::KeychainTestData keychain_data = {
      kSecAuthenticationTypeHTMLForm, "some.domain.com",
      kSecProtocolTypeHTTP, "/insecure.html", 0, NULL, "20020601171500Z",
      "joe_user", "sekrit", false };
  keychain_->AddTestItem(keychain_data);

  // Add a password through the adapter. It has the "Chrome" creator tag.
  // However, it's not referenced by the password database.
  MacKeychainPasswordFormAdapter owned_keychain_adapter(keychain_);
  owned_keychain_adapter.SetFindsOnlyOwnedItems(true);
  PasswordFormData www_form_data1 = {
      PasswordForm::SCHEME_HTML, "http://www.facebook.com/",
      "http://www.facebook.com/index.html", "login", L"username", L"password",
      L"submit", L"joe_user", L"sekrit", true, false, 1 };
  scoped_ptr<PasswordForm> www_form(CreatePasswordFormFromData(www_form_data1));
  EXPECT_TRUE(owned_keychain_adapter.AddPassword(*www_form));

  // Add a password from the current profile.
  PasswordFormData www_form_data2 = {
      PasswordForm::SCHEME_HTML, "http://www.facebook.com/",
      "http://www.facebook.com/index.html", "login", L"username", L"password",
      L"submit", L"not_joe_user", L"12345", true, false, 1 };
  www_form.reset(CreatePasswordFormFromData(www_form_data2));
  store_->AddLogin(*www_form);
  WaitForStoreUpdate();

  ScopedVector<PasswordForm> matching_items;
  login_db_->GetLogins(*www_form, &matching_items.get());
  EXPECT_EQ(1u, matching_items.size());
  matching_items.clear();

  store_->RemoveLoginsCreatedBetween(base::Time(), base::Time());
  WaitForStoreUpdate();

  // Check the second facebook form is gone.
  login_db_->GetLogins(*www_form, &matching_items.get());
  EXPECT_EQ(0u, matching_items.size());

  // Check the first facebook form is still there.
  matching_items.get() = owned_keychain_adapter.PasswordsFillingForm(
      www_form->signon_realm, www_form->scheme);
  ASSERT_EQ(1u, matching_items.size());
  EXPECT_EQ(ASCIIToUTF16("joe_user"), matching_items[0]->username_value);
  matching_items.clear();

  // Check the third-party password is still there.
  owned_keychain_adapter.SetFindsOnlyOwnedItems(false);
  matching_items.get() = owned_keychain_adapter.PasswordsFillingForm(
      "http://some.domain.com/insecure.html", PasswordForm::SCHEME_HTML);
  ASSERT_EQ(1u, matching_items.size());
}
