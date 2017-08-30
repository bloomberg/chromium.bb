// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_autofill_manager.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/user_action_tester.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion_test_helpers.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_f.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

// The name of the username/password element in the form.
const char kUsernameName[] = "username";
const char kInvalidUsername[] = "no-username";
const char kPasswordName[] = "password";

const char kAliceUsername[] = "alice";
const char kAlicePassword[] = "password";

using autofill::Suggestion;
using autofill::SuggestionVectorIdsAre;
using autofill::SuggestionVectorValuesAre;
using autofill::SuggestionVectorLabelsAre;
using testing::_;

namespace autofill {
class AutofillPopupDelegate;
}

namespace password_manager {

namespace {

constexpr char kMainFrameUrl[] = "https://example.com/";

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD2(FillSuggestion,
               void(const base::string16&, const base::string16&));
  MOCK_METHOD2(PreviewSuggestion,
               void(const base::string16&, const base::string16&));
  MOCK_METHOD0(GetPasswordManager, PasswordManager*());
};

class TestPasswordManagerClient : public StubPasswordManagerClient {
 public:
  TestPasswordManagerClient() : main_frame_url_(kMainFrameUrl) {}
  ~TestPasswordManagerClient() override = default;

  MockPasswordManagerDriver* mock_driver() { return &driver_; }
  const GURL& GetMainFrameURL() const override { return main_frame_url_; }

 private:
  MockPasswordManagerDriver driver_;
  GURL main_frame_url_;
};

class MockAutofillClient : public autofill::TestAutofillClient {
 public:
  MOCK_METHOD4(ShowAutofillPopup,
               void(const gfx::RectF& element_bounds,
                    base::i18n::TextDirection text_direction,
                    const std::vector<Suggestion>& suggestions,
                    base::WeakPtr<autofill::AutofillPopupDelegate> delegate));
  MOCK_METHOD0(HideAutofillPopup, void());
  MOCK_METHOD1(ExecuteCommand, void(int));
};

bool IsPreLollipopAndroid() {
#if defined(OS_ANDROID)
  return (base::android::BuildInfo::GetInstance()->sdk_int() <
          base::android::SDK_VERSION_LOLLIPOP);
#else
  return false;
#endif
}

}  // namespace

class PasswordAutofillManagerTest : public testing::Test {
 protected:
  PasswordAutofillManagerTest()
      : test_username_(base::ASCIIToUTF16(kAliceUsername)),
        test_password_(base::ASCIIToUTF16(kAlicePassword)),
        fill_data_id_(0) {}

  void SetUp() override {
    // Add a preferred login and an additional login to the FillData.
    autofill::FormFieldData username_field;
    username_field.name = base::ASCIIToUTF16(kUsernameName);
    username_field.value = test_username_;
    fill_data_.username_field = username_field;

    autofill::FormFieldData password_field;
    password_field.name = base::ASCIIToUTF16(kPasswordName);
    password_field.value = test_password_;
    fill_data_.password_field = password_field;
  }

  void InitializePasswordAutofillManager(
      TestPasswordManagerClient* client,
      autofill::AutofillClient* autofill_client) {
    password_autofill_manager_.reset(new PasswordAutofillManager(
        client->mock_driver(), autofill_client, client));
    password_autofill_manager_->OnAddPasswordFormMapping(fill_data_id_,
                                                         fill_data_);
  }

 protected:
  int fill_data_id() { return fill_data_id_; }
  autofill::PasswordFormFillData& fill_data() { return fill_data_; }

  void SetHttpWarningEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        security_state::kHttpFormWarningFeature);
  }

  void SetManualFallbacksForFilling(bool enabled) {
    if (enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          password_manager::features::kEnableManualFallbacksFilling);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          password_manager::features::kEnableManualFallbacksFilling);
    }
  }

  static bool IsManualFallbackForFillingEnabled() {
    return base::FeatureList::IsEnabled(
               password_manager::features::kEnableManualFallbacksFilling) &&
           !IsPreLollipopAndroid();
  }

  std::unique_ptr<PasswordAutofillManager> password_autofill_manager_;

  base::string16 test_username_;
  base::string16 test_password_;

 private:
  autofill::PasswordFormFillData fill_data_;
  const int fill_data_id_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // The TestAutofillDriver uses a SequencedWorkerPool which expects the
  // existence of a MessageLoop.
  base::MessageLoop message_loop_;
};

TEST_F(PasswordAutofillManagerTest, FillSuggestion) {
  std::unique_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient);
  InitializePasswordAutofillManager(client.get(), nullptr);

  EXPECT_CALL(*client->mock_driver(),
              FillSuggestion(test_username_, test_password_));
  EXPECT_TRUE(password_autofill_manager_->FillSuggestionForTest(
      fill_data_id(), test_username_));
  testing::Mock::VerifyAndClearExpectations(client->mock_driver());

  EXPECT_CALL(*client->mock_driver(), FillSuggestion(_, _)).Times(0);
  EXPECT_FALSE(password_autofill_manager_->FillSuggestionForTest(
      fill_data_id(), base::ASCIIToUTF16(kInvalidUsername)));

  const int invalid_fill_data_id = fill_data_id() + 1;

  EXPECT_FALSE(password_autofill_manager_->FillSuggestionForTest(
      invalid_fill_data_id, test_username_));

  password_autofill_manager_->DidNavigateMainFrame();
  EXPECT_FALSE(password_autofill_manager_->FillSuggestionForTest(
      fill_data_id(), test_username_));
}

TEST_F(PasswordAutofillManagerTest, PreviewSuggestion) {
  std::unique_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient);
  InitializePasswordAutofillManager(client.get(), nullptr);

  EXPECT_CALL(*client->mock_driver(),
              PreviewSuggestion(test_username_, test_password_));
  EXPECT_TRUE(password_autofill_manager_->PreviewSuggestionForTest(
      fill_data_id(), test_username_));
  testing::Mock::VerifyAndClearExpectations(client->mock_driver());

  EXPECT_CALL(*client->mock_driver(), PreviewSuggestion(_, _)).Times(0);
  EXPECT_FALSE(password_autofill_manager_->PreviewSuggestionForTest(
      fill_data_id(), base::ASCIIToUTF16(kInvalidUsername)));

  const int invalid_fill_data_id = fill_data_id() + 1;

  EXPECT_FALSE(password_autofill_manager_->PreviewSuggestionForTest(
      invalid_fill_data_id, test_username_));

  password_autofill_manager_->DidNavigateMainFrame();
  EXPECT_FALSE(password_autofill_manager_->PreviewSuggestionForTest(
      fill_data_id(), test_username_));
}

// Test that the popup is marked as visible after recieving password
// suggestions.
TEST_F(PasswordAutofillManagerTest, ExternalDelegatePasswordSuggestions) {
  for (bool is_suggestion_on_password_field : {false, true}) {
    std::unique_ptr<TestPasswordManagerClient> client(
        new TestPasswordManagerClient);
    std::unique_ptr<MockAutofillClient> autofill_client(new MockAutofillClient);
    InitializePasswordAutofillManager(client.get(), autofill_client.get());

    gfx::RectF element_bounds;
    autofill::PasswordFormFillData data;
    data.username_field.value = test_username_;
    data.password_field.value = test_password_;
    data.preferred_realm = "http://foo.com/";
    int dummy_key = 0;
    password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

    EXPECT_CALL(*client->mock_driver(),
                FillSuggestion(test_username_, test_password_));

    std::vector<autofill::PopupItemId> ids = {
        autofill::POPUP_ITEM_ID_USERNAME_ENTRY};
    if (is_suggestion_on_password_field) {
      ids = {autofill::POPUP_ITEM_ID_TITLE,
             autofill::POPUP_ITEM_ID_PASSWORD_ENTRY};
      if (IsManualFallbackForFillingEnabled()) {
#if !defined(OS_ANDROID)
        ids.push_back(autofill::POPUP_ITEM_ID_SEPARATOR);
#endif
        ids.push_back(autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY);
      }
    }
    EXPECT_CALL(
        *autofill_client,
        ShowAutofillPopup(
            _, _, SuggestionVectorIdsAre(testing::ElementsAreArray(ids)), _));

    int show_suggestion_options =
        is_suggestion_on_password_field ? autofill::IS_PASSWORD_FIELD : 0;
    password_autofill_manager_->OnShowPasswordSuggestions(
        dummy_key, base::i18n::RIGHT_TO_LEFT, base::string16(),
        show_suggestion_options, element_bounds);

    // Accepting a suggestion should trigger a call to hide the popup.
    EXPECT_CALL(*autofill_client, HideAutofillPopup());
    password_autofill_manager_->DidAcceptSuggestion(
        test_username_, is_suggestion_on_password_field
                            ? autofill::POPUP_ITEM_ID_PASSWORD_ENTRY
                            : autofill::POPUP_ITEM_ID_USERNAME_ENTRY,
        1);
  }
}

// Test that OnShowPasswordSuggestions correctly matches the given FormFieldData
// to the known PasswordFormFillData, and extracts the right suggestions.
TEST_F(PasswordAutofillManagerTest, ExtractSuggestions) {
  std::unique_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient);
  std::unique_ptr<MockAutofillClient> autofill_client(new MockAutofillClient);
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndRealm additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("John Foo"));
  data.additional_logins[additional_username] = additional;

  autofill::UsernamesCollectionKey usernames_key;
  usernames_key.realm = "http://yetanother.net";
  std::vector<base::string16> other_names;
  base::string16 other_username(base::ASCIIToUTF16("John Different"));
  other_names.push_back(other_username);
  data.other_possible_usernames[usernames_key] = other_names;

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  // First, simulate displaying suggestions matching an empty prefix. Also
  // verify that both the values and labels are filled correctly. The 'value'
  // should be the user name; the 'label' should be the realm.
  EXPECT_CALL(*autofill_client,
              ShowAutofillPopup(
                  element_bounds, _,
                  testing::AllOf(
                      SuggestionVectorValuesAre(testing::UnorderedElementsAre(
                          test_username_, additional_username, other_username)),
                      SuggestionVectorLabelsAre(testing::UnorderedElementsAre(
                          base::UTF8ToUTF16(data.preferred_realm),
                          base::UTF8ToUTF16(additional.realm),
                          base::UTF8ToUTF16(usernames_key.realm)))),
                  _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, base::string16(), false,
      element_bounds);

  // Now simulate displaying suggestions matching "John".
  EXPECT_CALL(*autofill_client,
              ShowAutofillPopup(
                  element_bounds, _,
                  SuggestionVectorValuesAre(testing::UnorderedElementsAre(
                      additional_username,
                      other_username)),
                  _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("John"), false,
      element_bounds);

  // Finally, simulate displaying all suggestions, without any prefix matching.
  EXPECT_CALL(*autofill_client,
              ShowAutofillPopup(
                  element_bounds, _,
                  SuggestionVectorValuesAre(testing::UnorderedElementsAre(
                      test_username_, additional_username, other_username)),
                  _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("xyz"), true,
      element_bounds);
}

// Verify that, for Android application credentials, the prettified realms of
// applications are displayed as the labels of suggestions on the UI (for
// matches of all levels of preferredness).
TEST_F(PasswordAutofillManagerTest, PrettifiedAndroidRealmsAreShownAsLabels) {
  std::unique_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient);
  std::unique_ptr<MockAutofillClient> autofill_client(new MockAutofillClient);
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.preferred_realm = "android://hash@com.example1.android/";

  autofill::PasswordAndRealm additional;
  additional.realm = "android://hash@com.example2.android/";
  base::string16 additional_username(base::ASCIIToUTF16("John Foo"));
  data.additional_logins[additional_username] = additional;

  autofill::UsernamesCollectionKey usernames_key;
  usernames_key.realm = "android://hash@com.example3.android/";
  std::vector<base::string16> other_names;
  base::string16 other_username(base::ASCIIToUTF16("John Different"));
  other_names.push_back(other_username);
  data.other_possible_usernames[usernames_key] = other_names;

  const int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(
          _, _, SuggestionVectorLabelsAre(testing::UnorderedElementsAre(
                    base::ASCIIToUTF16("android://com.example1.android/"),
                    base::ASCIIToUTF16("android://com.example2.android/"),
                    base::ASCIIToUTF16("android://com.example3.android/"))),
          _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, base::string16(), false,
      gfx::RectF());
}

TEST_F(PasswordAutofillManagerTest, FillSuggestionPasswordField) {
  std::unique_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient);
  std::unique_ptr<MockAutofillClient> autofill_client(new MockAutofillClient);
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndRealm additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("John Foo"));
  data.additional_logins[additional_username] = additional;

  autofill::UsernamesCollectionKey usernames_key;
  usernames_key.realm = "http://yetanother.net";
  std::vector<base::string16> other_names;
  base::string16 other_username(base::ASCIIToUTF16("John Different"));
  other_names.push_back(other_username);
  data.other_possible_usernames[usernames_key] = other_names;

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  // Simulate displaying suggestions matching a username and specifying that the
  // field is a password field.
  base::string16 title = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PASSWORD_FIELD_SUGGESTIONS_TITLE);
  std::vector<base::string16> elements = {title, test_username_};
  if (IsManualFallbackForFillingEnabled()) {
    elements = {
      title,
      test_username_,
#if !defined(OS_ANDROID)
      base::string16(),
#endif
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ALL_SAVED_FALLBACK)
    };
  }

  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(
          element_bounds, _,
          SuggestionVectorValuesAre(testing::ElementsAreArray(elements)), _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::IS_PASSWORD_FIELD, element_bounds);
}

// Verify that typing "foo" into the username field will match usernames
// "foo.bar@example.com", "bar.foo@example.com" and "example@foo.com".
TEST_F(PasswordAutofillManagerTest, DisplaySuggestionsWithMatchingTokens) {
  // Token matching is currently behind a flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kEnableSuggestionsWithSubstringMatch);

  std::unique_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient);
  std::unique_ptr<MockAutofillClient> autofill_client(new MockAutofillClient);
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  base::string16 username = base::ASCIIToUTF16("foo.bar@example.com");
  data.username_field.value = username;
  data.password_field.value = base::ASCIIToUTF16("foobar");
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndRealm additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("bar.foo@example.com"));
  data.additional_logins[additional_username] = additional;

  autofill::UsernamesCollectionKey usernames_key;
  usernames_key.realm = "http://yetanother.net";
  std::vector<base::string16> other_names;
  base::string16 other_username(base::ASCIIToUTF16("example@foo.com"));
  other_names.push_back(other_username);
  data.other_possible_usernames[usernames_key] = other_names;

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(element_bounds, _,
                        SuggestionVectorValuesAre(testing::UnorderedElementsAre(
                            username, additional_username, other_username)),
                        _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("foo"), false,
      element_bounds);
}

// Verify that typing "oo" into the username field will not match any usernames
// "foo.bar@example.com", "bar.foo@example.com" or "example@foo.com".
TEST_F(PasswordAutofillManagerTest, NoSuggestionForNonPrefixTokenMatch) {
  // Token matching is currently behind a flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kEnableSuggestionsWithSubstringMatch);

  std::unique_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient);
  std::unique_ptr<MockAutofillClient> autofill_client(new MockAutofillClient);
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  base::string16 username = base::ASCIIToUTF16("foo.bar@example.com");
  data.username_field.value = username;
  data.password_field.value = base::ASCIIToUTF16("foobar");
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndRealm additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("bar.foo@example.com"));
  data.additional_logins[additional_username] = additional;

  autofill::UsernamesCollectionKey usernames_key;
  usernames_key.realm = "http://yetanother.net";
  std::vector<base::string16> other_names;
  base::string16 other_username(base::ASCIIToUTF16("example@foo.com"));
  other_names.push_back(other_username);
  data.other_possible_usernames[usernames_key] = other_names;

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  EXPECT_CALL(*autofill_client, ShowAutofillPopup(_, _, _, _)).Times(0);

  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("oo"), false,
      element_bounds);
}

// Verify that typing "foo@exam" into the username field will match username
// "bar.foo@example.com" even if the field contents span accross multiple
// tokens.
TEST_F(PasswordAutofillManagerTest,
       MatchingContentsWithSuggestionTokenSeparator) {
  // Token matching is currently behind a flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kEnableSuggestionsWithSubstringMatch);

  std::unique_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient);
  std::unique_ptr<MockAutofillClient> autofill_client(new MockAutofillClient);
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  base::string16 username = base::ASCIIToUTF16("foo.bar@example.com");
  data.username_field.value = username;
  data.password_field.value = base::ASCIIToUTF16("foobar");
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndRealm additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("bar.foo@example.com"));
  data.additional_logins[additional_username] = additional;

  autofill::UsernamesCollectionKey usernames_key;
  usernames_key.realm = "http://yetanother.net";
  std::vector<base::string16> other_names;
  base::string16 other_username(base::ASCIIToUTF16("example@foo.com"));
  other_names.push_back(other_username);
  data.other_possible_usernames[usernames_key] = other_names;

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(element_bounds, _,
                        SuggestionVectorValuesAre(
                            testing::UnorderedElementsAre(additional_username)),
                        _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("foo@exam"),
      false, element_bounds);
}

// Verify that typing "example" into the username field will match and order
// usernames "example@foo.com", "foo.bar@example.com" and "bar.foo@example.com"
// i.e. prefix matched followed by substring matched.
TEST_F(PasswordAutofillManagerTest,
       DisplaySuggestionsWithPrefixesPrecedeSubstringMatched) {
  // Token matching is currently behind a flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kEnableSuggestionsWithSubstringMatch);

  std::unique_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient);
  std::unique_ptr<MockAutofillClient> autofill_client(new MockAutofillClient);
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  base::string16 username = base::ASCIIToUTF16("foo.bar@example.com");
  data.username_field.value = username;
  data.password_field.value = base::ASCIIToUTF16("foobar");
  data.preferred_realm = "http://foo.com/";

  autofill::PasswordAndRealm additional;
  additional.realm = "https://foobarrealm.org";
  base::string16 additional_username(base::ASCIIToUTF16("bar.foo@example.com"));
  data.additional_logins[additional_username] = additional;

  autofill::UsernamesCollectionKey usernames_key;
  usernames_key.realm = "http://yetanother.net";
  std::vector<base::string16> other_names;
  base::string16 other_username(base::ASCIIToUTF16("example@foo.com"));
  other_names.push_back(other_username);
  data.other_possible_usernames[usernames_key] = other_names;

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(element_bounds, _,
                        SuggestionVectorValuesAre(testing::UnorderedElementsAre(
                            other_username, username, additional_username)),
                        _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, base::ASCIIToUTF16("foo"), false,
      element_bounds);
}

TEST_F(PasswordAutofillManagerTest, PreviewAndFillEmptyUsernameSuggestion) {
  // Initialize PasswordAutofillManager with credentials without username.
  std::unique_ptr<TestPasswordManagerClient> client(
      new TestPasswordManagerClient);
  std::unique_ptr<MockAutofillClient> autofill_client(new MockAutofillClient);
  fill_data().username_field.value.clear();
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  base::string16 no_username_string =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);

  // Simulate that the user clicks on a username field.
  EXPECT_CALL(*autofill_client, ShowAutofillPopup(_, _, _, _));
  gfx::RectF element_bounds;
  password_autofill_manager_->OnShowPasswordSuggestions(
      fill_data_id(), base::i18n::RIGHT_TO_LEFT, base::string16(), false,
      element_bounds);

  // Check that preview of the empty username works.
  EXPECT_CALL(*client->mock_driver(),
              PreviewSuggestion(base::string16(), test_password_));
  password_autofill_manager_->DidSelectSuggestion(no_username_string,
                                                  0 /*not used*/);
  testing::Mock::VerifyAndClearExpectations(client->mock_driver());

  // Check that fill of the empty username works.
  EXPECT_CALL(*client->mock_driver(),
              FillSuggestion(base::string16(), test_password_));
  EXPECT_CALL(*autofill_client, HideAutofillPopup());
  password_autofill_manager_->DidAcceptSuggestion(
      no_username_string, autofill::POPUP_ITEM_ID_PASSWORD_ENTRY, 1);
  testing::Mock::VerifyAndClearExpectations(client->mock_driver());
}

// Tests that a standalone Form Not Secure warning shows up in the
// autofill popup when PasswordAutofillManager::OnShowNotSecureWarning()
// is called.
TEST_F(PasswordAutofillManagerTest, ShowStandaloneNotSecureWarning) {
  auto client = base::MakeUnique<TestPasswordManagerClient>();
  auto autofill_client = base::MakeUnique<MockAutofillClient>();
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.origin = GURL("http://foo.test");

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  // String "Login not secure" shown as a warning messages if password form is
  // on http sites.
  const base::string16 warning_message =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_LOGIN_HTTP_WARNING_MESSAGE);

  SetHttpWarningEnabled();

  auto elements = testing::ElementsAre(warning_message);

  EXPECT_CALL(*autofill_client,
              ShowAutofillPopup(element_bounds, _,
                                SuggestionVectorValuesAre(elements), _));
  password_autofill_manager_->OnShowNotSecureWarning(base::i18n::RIGHT_TO_LEFT,
                                                     element_bounds);

  // Accepting the warning message should trigger a call to open an explanation
  // of the message and hide the popup.
  EXPECT_CALL(
      *autofill_client,
      ExecuteCommand(autofill::POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE));
  EXPECT_CALL(*autofill_client, HideAutofillPopup());
  password_autofill_manager_->DidAcceptSuggestion(
      base::string16(), autofill::POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE,
      0);
}

TEST_F(PasswordAutofillManagerTest, NonSecurePasswordFieldHttpWarningMessage) {
  auto client = base::MakeUnique<TestPasswordManagerClient>();
  auto autofill_client = base::MakeUnique<MockAutofillClient>();
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.origin = GURL("http://foo.test");

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  // String "Login not secure" shown as a warning messages if password form is
  // on http sites.
  base::string16 warning_message =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_LOGIN_HTTP_WARNING_MESSAGE);

  // String "Use password for:" shown when displaying suggestions matching a
  // username and specifying that the field is a password field.
  base::string16 title =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_PASSWORD_FIELD_SUGGESTIONS_TITLE);

  base::string16 show_all_saved_row_text =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ALL_SAVED_FALLBACK);
  std::vector<base::string16> elements = {title, test_username_};
  if (IsManualFallbackForFillingEnabled()) {
#if !defined(OS_ANDROID)
    elements.push_back(base::string16());
#endif
    elements.push_back(show_all_saved_row_text);
  }

  // Http warning message won't show with switch flag off.
  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(
          element_bounds, _,
          SuggestionVectorValuesAre(testing::ElementsAreArray(elements)), _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::IS_PASSWORD_FIELD, element_bounds);

  SetHttpWarningEnabled();

  // Http warning message shows for non-secure context and switch flag on, so
  // there are 3 suggestions (+ 1 separator on desktop) in total, and the
  // message comes first among suggestions.
  elements = {
    warning_message,
#if !defined(OS_ANDROID)
    base::string16(),
#endif
    title,
    test_username_
  };
  if (IsManualFallbackForFillingEnabled()) {
#if !defined(OS_ANDROID)
    elements.push_back(base::string16());
#endif
    elements.push_back(show_all_saved_row_text);
  }

  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(
          element_bounds, _,
          SuggestionVectorValuesAre(testing::ElementsAreArray(elements)), _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::IS_PASSWORD_FIELD, element_bounds);

  // Accepting the warning message should trigger a call to open an explanation
  // of the message and hide the popup.
  EXPECT_CALL(
      *autofill_client,
      ExecuteCommand(autofill::POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE));
  EXPECT_CALL(*autofill_client, HideAutofillPopup());
  password_autofill_manager_->DidAcceptSuggestion(
      base::string16(), autofill::POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE,
      0);
}

// Tests that the "Login not secure" warning shows up in non-password
// fields of login forms.
TEST_F(PasswordAutofillManagerTest, NonSecureUsernameFieldHttpWarningMessage) {
  auto client = base::MakeUnique<TestPasswordManagerClient>();
  auto autofill_client = base::MakeUnique<MockAutofillClient>();
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.origin = GURL("http://foo.test");

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  // String "Login not secure" shown as a warning messages if password form is
  // on http sites.
  base::string16 warning_message =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_LOGIN_HTTP_WARNING_MESSAGE);

  // Http warning message won't show with switch flag off.
  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(
          element_bounds, _,
          SuggestionVectorValuesAre(testing::ElementsAre(test_username_)), _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, test_username_, 0, element_bounds);

  SetHttpWarningEnabled();

  // Http warning message shows for non-secure context and switch flag on, so
  // there are 2 suggestions (+ 1 separator on desktop) in total, and the
  // message comes first among suggestions.
  auto elements = testing::ElementsAre(warning_message,
#if !defined(OS_ANDROID)
                                       base::string16(),
#endif
                                       test_username_);

  EXPECT_CALL(*autofill_client,
              ShowAutofillPopup(element_bounds, _,
                                SuggestionVectorValuesAre(elements), _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, test_username_, 0, element_bounds);

  // Accepting the warning message should trigger a call to open an explanation
  // of the message and hide the popup.
  EXPECT_CALL(
      *autofill_client,
      ExecuteCommand(autofill::POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE));
  EXPECT_CALL(*autofill_client, HideAutofillPopup());
  password_autofill_manager_->DidAcceptSuggestion(
      base::string16(), autofill::POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE,
      0);
}

TEST_F(PasswordAutofillManagerTest, SecurePasswordFieldHttpWarningMessage) {
  auto client = base::MakeUnique<TestPasswordManagerClient>();
  auto autofill_client = base::MakeUnique<MockAutofillClient>();
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.origin = GURL("https://foo.test");

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  // String "Use password for:" shown when displaying suggestions matching a
  // username and specifying that the field is a password field.
  base::string16 title =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_PASSWORD_FIELD_SUGGESTIONS_TITLE);

  std::vector<base::string16> elements = {title, test_username_};
  if (IsManualFallbackForFillingEnabled()) {
    elements = {
      title,
      test_username_,
#if !defined(OS_ANDROID)
      base::string16(),
#endif
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ALL_SAVED_FALLBACK)
    };
  }
  // Http warning message won't show with switch flag off.
  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(
          element_bounds, _,
          SuggestionVectorValuesAre(testing::ElementsAreArray(elements)), _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::IS_PASSWORD_FIELD, element_bounds);

  SetHttpWarningEnabled();

  // Http warning message won't show for secure context, even with switch flag
  // on.
  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(
          element_bounds, _,
          SuggestionVectorValuesAre(testing::ElementsAreArray(elements)), _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::IS_PASSWORD_FIELD, element_bounds);
}

// Tests that the Form-Not-Secure warning is recorded in UMA, at most once per
// navigation.
TEST_F(PasswordAutofillManagerTest, ShowedFormNotSecureHistogram) {
  const char kHistogram[] =
      "PasswordManager.ShowedFormNotSecureWarningOnCurrentNavigation";
  base::HistogramTester histograms;
  SetHttpWarningEnabled();

  auto client = base::MakeUnique<TestPasswordManagerClient>();
  auto autofill_client = base::MakeUnique<MockAutofillClient>();
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  // Test that the standalone warning (with no autofill suggestions) records the
  // histogram.
  gfx::RectF element_bounds;
  EXPECT_CALL(*autofill_client, ShowAutofillPopup(element_bounds, _, _, _));
  password_autofill_manager_->OnShowNotSecureWarning(base::i18n::RIGHT_TO_LEFT,
                                                     element_bounds);
  histograms.ExpectUniqueSample(kHistogram, true, 1);

  // Simulate a navigation, because the histogram is recorded at most once per
  // navigation.
  password_autofill_manager_->DidNavigateMainFrame();

  // Test that the warning, when included with the normal autofill dropdown,
  // records the histogram.
  int dummy_key = 0;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.origin = GURL("http://foo.test");
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  EXPECT_CALL(*autofill_client, ShowAutofillPopup(element_bounds, _, _, _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::IS_PASSWORD_FIELD, element_bounds);
  histograms.ExpectUniqueSample(kHistogram, true, 2);

  // The histogram should not be recorded again on the same navigation.
  EXPECT_CALL(*autofill_client, ShowAutofillPopup(element_bounds, _, _, _));
  password_autofill_manager_->OnShowNotSecureWarning(base::i18n::RIGHT_TO_LEFT,
                                                     element_bounds);
  histograms.ExpectUniqueSample(kHistogram, true, 2);
}

// Tests that the "Show all passwords" suggestion isn't shown along with
// "Use password for" in the popup when the feature which controls its
// appearance is disabled.
TEST_F(PasswordAutofillManagerTest,
       NotShowAllPasswordsOptionOnPasswordFieldWhenFeatureDisabled) {
  auto client = base::MakeUnique<TestPasswordManagerClient>();
  auto autofill_client = base::MakeUnique<MockAutofillClient>();
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.origin = GURL("https://foo.test");

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  // String "Use password for:" shown when displaying suggestions matching a
  // username and specifying that the field is a password field.
  base::string16 title =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_PASSWORD_FIELD_SUGGESTIONS_TITLE);

  SetManualFallbacksForFilling(false);

  // No "Show all passwords row" when feature is disabled.
  EXPECT_CALL(*autofill_client,
              ShowAutofillPopup(element_bounds, _,
                                SuggestionVectorValuesAre(testing::ElementsAre(
                                    title, test_username_)),
                                _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::IS_PASSWORD_FIELD, element_bounds);
}

// Tests that the "Show all passwords" suggestion is shown along with
// "Use password for" in the popup when the feature which controls its
// appearance is enabled.
TEST_F(PasswordAutofillManagerTest, ShowAllPasswordsOptionOnPasswordField) {
  const char kShownContextHistogram[] =
      "PasswordManager.ShowAllSavedPasswordsShownContext";
  const char kAcceptedContextHistogram[] =
      "PasswordManager.ShowAllSavedPasswordsAcceptedContext";
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto client = base::MakeUnique<TestPasswordManagerClient>();
  auto autofill_client = base::MakeUnique<MockAutofillClient>();
  auto manager =
      base::MakeUnique<password_manager::PasswordManager>(client.get());
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  ON_CALL(*(client->mock_driver()), GetPasswordManager())
      .WillByDefault(testing::Return(manager.get()));

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.origin = GURL("https://foo.test");

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  // String "Use password for:" shown when displaying suggestions matching a
  // username and specifying that the field is a password field.
  base::string16 title =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_PASSWORD_FIELD_SUGGESTIONS_TITLE);

  SetManualFallbacksForFilling(true);

  std::vector<base::string16> elements = {title, test_username_};
  if (!IsPreLollipopAndroid()) {
    elements = {
      title,
      test_username_,
#if !defined(OS_ANDROID)
      base::string16(),
#endif
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ALL_SAVED_FALLBACK)
    };
  }

  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(
          element_bounds, _,
          SuggestionVectorValuesAre(testing::ElementsAreArray(elements)), _));

  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, test_username_,
      autofill::IS_PASSWORD_FIELD, element_bounds);

  if (!IsPreLollipopAndroid()) {
    // Expect a sample only in the shown histogram.
    histograms.ExpectUniqueSample(
        kShownContextHistogram,
        metrics_util::SHOW_ALL_SAVED_PASSWORDS_CONTEXT_PASSWORD, 1);
    // Clicking at the "Show all passwords row" should trigger a call to open
    // the Password Manager settings page and hide the popup.
    EXPECT_CALL(
        *autofill_client,
        ExecuteCommand(autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY));
    EXPECT_CALL(*autofill_client, HideAutofillPopup());
    password_autofill_manager_->DidAcceptSuggestion(
        base::string16(), autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY, 0);
    // Expect a sample in both the shown and accepted histogram.
    histograms.ExpectUniqueSample(
        kShownContextHistogram,
        metrics_util::SHOW_ALL_SAVED_PASSWORDS_CONTEXT_PASSWORD, 1);
    histograms.ExpectUniqueSample(
        kAcceptedContextHistogram,
        metrics_util::SHOW_ALL_SAVED_PASSWORDS_CONTEXT_PASSWORD, 1);
    // Trigger UKM reporting, which happens at destruction time.
    manager.reset();
    autofill_client.reset();
    client.reset();
    const ukm::UkmSource* source =
        test_ukm_recorder.GetSourceForUrl(kMainFrameUrl);
    ASSERT_TRUE(source);

    test_ukm_recorder.ExpectMetric(
        *source, "PageWithPassword", password_manager::kUkmPageLevelUserAction,
        static_cast<int64_t>(
            password_manager::PasswordManagerMetricsRecorder::
                PageLevelUserAction::kShowAllPasswordsWhileSomeAreSuggested));

  } else {
    EXPECT_THAT(histograms.GetAllSamples(kShownContextHistogram),
                testing::IsEmpty());
    EXPECT_THAT(histograms.GetAllSamples(kAcceptedContextHistogram),
                testing::IsEmpty());
  }
}

TEST_F(PasswordAutofillManagerTest, ShowStandaloneShowAllPasswords) {
  const char kShownContextHistogram[] =
      "PasswordManager.ShowAllSavedPasswordsShownContext";
  const char kAcceptedContextHistogram[] =
      "PasswordManager.ShowAllSavedPasswordsAcceptedContext";
  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  auto client = base::MakeUnique<TestPasswordManagerClient>();
  auto autofill_client = base::MakeUnique<MockAutofillClient>();
  auto manager =
      base::MakeUnique<password_manager::PasswordManager>(client.get());
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  ON_CALL(*(client->mock_driver()), GetPasswordManager())
      .WillByDefault(testing::Return(manager.get()));

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.origin = GURL("http://foo.test");

  // String for the "Show all passwords" fallback.
  base::string16 show_all_saved_row_text =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ALL_SAVED_FALLBACK);

  SetManualFallbacksForFilling(true);

  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(element_bounds, _,
                        SuggestionVectorValuesAre(testing::ElementsAreArray(
                            {show_all_saved_row_text})),
                        _))
      .Times(IsPreLollipopAndroid() ? 0 : 1);
  password_autofill_manager_->OnShowManualFallbackSuggestion(
      base::i18n::RIGHT_TO_LEFT, element_bounds);

  if (IsPreLollipopAndroid()) {
    EXPECT_THAT(histograms.GetAllSamples(kShownContextHistogram),
                testing::IsEmpty());
  } else {
    // Expect a sample only in the shown histogram.
    histograms.ExpectUniqueSample(
        kShownContextHistogram,
        metrics_util::SHOW_ALL_SAVED_PASSWORDS_CONTEXT_MANUAL_FALLBACK, 1);
  }

  if (!IsPreLollipopAndroid()) {
    // Clicking at the "Show all passwords row" should trigger a call to open
    // the Password Manager settings page and hide the popup.
    EXPECT_CALL(
        *autofill_client,
        ExecuteCommand(autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY));
    EXPECT_CALL(*autofill_client, HideAutofillPopup());
    password_autofill_manager_->DidAcceptSuggestion(
        base::string16(), autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY, 0);
    // Expect a sample in both the shown and accepted histogram.
    histograms.ExpectUniqueSample(
        kShownContextHistogram,
        metrics_util::SHOW_ALL_SAVED_PASSWORDS_CONTEXT_MANUAL_FALLBACK, 1);
    histograms.ExpectUniqueSample(
        kAcceptedContextHistogram,
        metrics_util::SHOW_ALL_SAVED_PASSWORDS_CONTEXT_MANUAL_FALLBACK, 1);
    // Trigger UKM reporting, which happens at destruction time.
    manager.reset();
    autofill_client.reset();
    client.reset();
    const ukm::UkmSource* source =
        test_ukm_recorder.GetSourceForUrl(kMainFrameUrl);
    ASSERT_TRUE(source);

    test_ukm_recorder.ExpectMetric(
        *source, "PageWithPassword", password_manager::kUkmPageLevelUserAction,
        static_cast<int64_t>(
            password_manager::PasswordManagerMetricsRecorder::
                PageLevelUserAction::kShowAllPasswordsWhileNoneAreSuggested));
  } else {
    EXPECT_THAT(histograms.GetAllSamples(kShownContextHistogram),
                testing::IsEmpty());
    EXPECT_THAT(histograms.GetAllSamples(kAcceptedContextHistogram),
                testing::IsEmpty());
  }
}

// Tests that the "Show all passwords" fallback doesn't shows up in non-password
// fields of login forms.
TEST_F(PasswordAutofillManagerTest,
       NotShowAllPasswordsOptionOnNonPasswordField) {
  auto client = base::MakeUnique<TestPasswordManagerClient>();
  auto autofill_client = base::MakeUnique<MockAutofillClient>();
  InitializePasswordAutofillManager(client.get(), autofill_client.get());

  gfx::RectF element_bounds;
  autofill::PasswordFormFillData data;
  data.username_field.value = test_username_;
  data.password_field.value = test_password_;
  data.origin = GURL("https://foo.test");

  int dummy_key = 0;
  password_autofill_manager_->OnAddPasswordFormMapping(dummy_key, data);

  SetManualFallbacksForFilling(true);

  EXPECT_CALL(
      *autofill_client,
      ShowAutofillPopup(
          element_bounds, _,
          SuggestionVectorValuesAre(testing::ElementsAre(test_username_)), _));
  password_autofill_manager_->OnShowPasswordSuggestions(
      dummy_key, base::i18n::RIGHT_TO_LEFT, test_username_, 0, element_bounds);
}

// SimpleWebviewDialog doesn't have an autofill client. Nothing should crash if
// the filling fallback is invoked.
TEST_F(PasswordAutofillManagerTest, ShowAllPasswordsWithoutAutofillClient) {
  auto client = base::MakeUnique<TestPasswordManagerClient>();
  InitializePasswordAutofillManager(client.get(), nullptr);

  SetManualFallbacksForFilling(true);

  password_autofill_manager_->OnShowManualFallbackSuggestion(
      base::i18n::RIGHT_TO_LEFT, gfx::RectF());
}

}  // namespace password_manager
