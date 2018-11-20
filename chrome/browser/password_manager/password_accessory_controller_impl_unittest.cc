// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_accessory_controller_impl.h"

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/password_manager/password_generation_dialog_view_interface.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"

namespace {
using autofill::AccessorySheetData;
using autofill::FillingStatus;
using autofill::FooterCommand;
using autofill::PasswordForm;
using autofill::UserInfo;
using autofill::password_generation::PasswordGenerationUIData;
using base::ASCIIToUTF16;
using testing::_;
using testing::ByMove;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

constexpr char kExampleSite[] = "https://example.com";
constexpr char kExampleDomain[] = "example.com";
constexpr int kIconSize = 75;  // An example size for favicons (=> 3.5*20px).

class MockManualFillingController
    : public ManualFillingController,
      public base::SupportsWeakPtr<MockManualFillingController> {
 public:
  MOCK_METHOD1(OnAutomaticGenerationStatusChanged, void(bool));
  MOCK_METHOD1(OnFilledIntoFocusedField, void(FillingStatus));
  MOCK_METHOD2(RefreshSuggestionsForField,
               void(bool, const AccessorySheetData&));
  MOCK_METHOD0(ShowWhenKeyboardIsVisible, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD2(GetFavicon,
               void(int, base::OnceCallback<void(const gfx::Image&)>));
  MOCK_METHOD2(OnFillingTriggered,
               void(bool is_password, const base::string16&));
  MOCK_CONST_METHOD1(OnOptionSelected, void(const base::string16&));
  MOCK_METHOD0(OnGenerationRequested, void());
  MOCK_CONST_METHOD0(container_view, gfx::NativeView());
};

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() = default;

  MOCK_METHOD0(GetPasswordGenerationManager,
               password_manager::PasswordGenerationManager*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerDriver);
};

class MockPasswordGenerationManager
    : public password_manager::PasswordGenerationManager {
 public:
  MockPasswordGenerationManager(password_manager::PasswordManagerClient* client,
                                password_manager::PasswordManagerDriver* driver)
      : password_manager::PasswordGenerationManager(client, driver) {}

  MOCK_METHOD5(GeneratePassword,
               base::string16(const GURL&,
                              autofill::FormSignature,
                              autofill::FieldSignature,
                              uint32_t,
                              uint32_t*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordGenerationManager);
};

// Mock modal dialog view used to bypass the need of a valid top level window.
class MockPasswordGenerationDialogView
    : public PasswordGenerationDialogViewInterface {
 public:
  MockPasswordGenerationDialogView() = default;

  MOCK_METHOD1(Show, void(base::string16&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordGenerationDialogView);
};

// Helper class for AccessorySheetData objects creation.
//
// Example that creates a AccessorySheetData object with two UserInfo objects;
// the former has two fields, whereas the latter has three fields:
//   AccessorySheetData data = AccessorySheetDataBuilder(title)
//       .AddUserInfo()
//           .AppendField(...)
//           .AppendField(...)
//       .AddUserInfo()
//           .AppendField(...)
//           .AppendField(...)
//           .AppendField(...)
//       .Build();
class AccessorySheetDataBuilder {
 public:
  explicit AccessorySheetDataBuilder(const base::string16& title)
      : accessory_sheet_data_(title) {}

  ~AccessorySheetDataBuilder() = default;

  // Adds a new UserInfo object to |accessory_sheet_data_|.
  AccessorySheetDataBuilder& AddUserInfo() {
    accessory_sheet_data_.add_user_info(UserInfo());
    return *this;
  }

  // Appends a field to the last UserInfo object.
  AccessorySheetDataBuilder& AppendField(const base::string16& display_text,
                                         const base::string16& a11y_description,
                                         bool is_obfuscated,
                                         bool selectable) {
    accessory_sheet_data_.mutable_user_info_list().back().add_field(
        UserInfo::Field(display_text, a11y_description, is_obfuscated,
                        selectable));
    return *this;
  }

  // Appends a new footer command to |accessory_sheet_data_|.
  AccessorySheetDataBuilder& AppendFooterCommand(
      const base::string16& display_text) {
    accessory_sheet_data_.add_footer_command(FooterCommand(display_text));
    return *this;
  }

  const AccessorySheetData& Build() { return accessory_sheet_data_; }

 private:
  AccessorySheetData accessory_sheet_data_;
};

// Creates a new map entry in the |first| element of the returned pair. The
// |second| element holds the PasswordForm that the |first| element points to.
// That way, the pointer only points to a valid address in the called scope.
std::pair<std::pair<base::string16, const PasswordForm*>,
          std::unique_ptr<const PasswordForm>>
CreateEntry(const std::string& username, const std::string& password) {
  PasswordForm form;
  form.username_value = ASCIIToUTF16(username);
  form.password_value = ASCIIToUTF16(password);
  std::unique_ptr<const PasswordForm> form_ptr(
      new PasswordForm(std::move(form)));
  auto username_form_pair =
      std::make_pair(ASCIIToUTF16(username), form_ptr.get());
  return {std::move(username_form_pair), std::move(form_ptr)};
}

base::string16 password_for_str(const base::string16& user) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, user);
}

base::string16 password_for_str(const std::string& user) {
  return password_for_str(ASCIIToUTF16(user));
}

base::string16 passwords_empty_str(const std::string& domain) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE,
      ASCIIToUTF16(domain));
}

base::string16 passwords_title_str(const std::string& domain) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE, ASCIIToUTF16(domain));
}

base::string16 no_user_str() {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
}

base::string16 manage_passwords_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK);
}

// Creates a AccessorySheetDataBuilder object with a "Manage passwords..."
// footer.
AccessorySheetDataBuilder PasswordAccessorySheetDataBuilder(
    const base::string16& title) {
  AccessorySheetDataBuilder builder(title);
  builder.AppendFooterCommand(manage_passwords_str());
  return builder;
}

PasswordGenerationUIData GetTestGenerationUIData1() {
  PasswordForm form;
  form.form_data = autofill::FormData();
  form.form_data.action = GURL("http://www.example1.com/accounts/Login");
  form.form_data.origin = GURL("http://www.example1.com/accounts/LoginAuth");
  PasswordGenerationUIData data;
  data.password_form = form;
  data.generation_element = ASCIIToUTF16("testelement1");
  data.max_length = 10;
  return data;
}

PasswordGenerationUIData GetTestGenerationUIData2() {
  PasswordForm form;
  form.form_data = autofill::FormData();
  form.form_data.action = GURL("http://www.example2.com/accounts/Login");
  form.form_data.origin = GURL("http://www.example2.com/accounts/LoginAuth");
  PasswordGenerationUIData data;
  data.password_form = form;
  data.generation_element = ASCIIToUTF16("testelement2");
  data.max_length = 11;
  return data;
}

}  // namespace

// Automagically used to pretty-print UserInfo::Field. Must be in same
// namespace.
void PrintTo(const UserInfo::Field& field, std::ostream* os) {
  *os << "(display text: \"" << base::UTF16ToUTF8(field.display_text())
      << "\", a11y_description: \""
      << base::UTF16ToUTF8(field.a11y_description()) << "\", is "
      << (field.is_obfuscated() ? "" : "not ") << "obfuscated, is "
      << (field.selectable() ? "" : "not ") << "selectable)";
}

// Automagically used to pretty-print UserInfo. Must be in same namespace.
void PrintTo(const UserInfo& user_info, std::ostream* os) {
  *os << "[";
  for (const UserInfo::Field& field : user_info.fields()) {
    PrintTo(field, os);
    *os << ", ";
  }
  *os << "]";
}

// Automagically used to pretty-print FooterCommand. Must be in same namespace.
void PrintTo(const FooterCommand& footer_command, std::ostream* os) {
  *os << "(display text: \"" << base::UTF16ToUTF8(footer_command.display_text())
      << "\")";
}

// Automagically used to pretty-print AccessorySheetData. Must be in same
// namespace.
void PrintTo(const AccessorySheetData& data, std::ostream* os) {
  *os << "has title: \"" << data.title() << "\", has user info list: [";
  for (const UserInfo& user_info : data.user_info_list()) {
    PrintTo(user_info, os);
    *os << ", ";
  }
  *os << "], has footer commands: [";
  for (const FooterCommand& footer_command : data.footer_commands()) {
    PrintTo(footer_command, os);
    *os << ", ";
  }
  *os << "]";
}

class PasswordAccessoryControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  PasswordAccessoryControllerTest()
      : mock_favicon_service_(
            std::make_unique<StrictMock<favicon::MockFaviconService>>()) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kExampleSite));
    PasswordAccessoryControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_manual_filling_controller_.AsWeakPtr(),
        mock_dialog_factory_.Get(), favicon_service());
    NavigateAndCommit(GURL(kExampleSite));

    // Mock objects needed by password generation
    mock_password_manager_driver_ =
        std::make_unique<NiceMock<MockPasswordManagerDriver>>();
    mock_generation_manager_ =
        std::make_unique<NiceMock<MockPasswordGenerationManager>>(
            nullptr, mock_password_manager_driver_.get());
    mock_dialog_ =
        std::make_unique<NiceMock<MockPasswordGenerationDialogView>>();
  }

  PasswordAccessoryController* controller() {
    return PasswordAccessoryControllerImpl::FromWebContents(web_contents());
  }

  const base::MockCallback<
      PasswordAccessoryControllerImpl::CreateDialogFactory>&
  mock_dialog_factory() {
    return mock_dialog_factory_;
  }

  favicon::MockFaviconService* favicon_service() {
    return mock_favicon_service_.get();
  }

 protected:
  // Sets up mocks needed by the generation flow and signals the
  // |PasswordAccessoryControllerImpl| that generation is available.
  void InitializeGeneration(const base::string16& password);

  StrictMock<MockManualFillingController> mock_manual_filling_controller_;

  std::unique_ptr<NiceMock<MockPasswordManagerDriver>>
      mock_password_manager_driver_;
  std::unique_ptr<NiceMock<MockPasswordGenerationManager>>
      mock_generation_manager_;
  std::unique_ptr<NiceMock<MockPasswordGenerationDialogView>> mock_dialog_;

 private:
  NiceMock<
      base::MockCallback<PasswordAccessoryControllerImpl::CreateDialogFactory>>
      mock_dialog_factory_;
  std::unique_ptr<StrictMock<favicon::MockFaviconService>>
      mock_favicon_service_;
};

void PasswordAccessoryControllerTest::InitializeGeneration(
    const base::string16& password) {
  ON_CALL(*(mock_password_manager_driver_.get()),
          GetPasswordGenerationManager())
      .WillByDefault(Return(mock_generation_manager_.get()));

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(true));

  controller()->OnAutomaticGenerationStatusChanged(
      true, GetTestGenerationUIData1(),
      mock_password_manager_driver_->AsWeakPtr());

  ON_CALL(*(mock_generation_manager_.get()), GeneratePassword(_, _, _, _, _))
      .WillByDefault(Return(password));

  ON_CALL(mock_dialog_factory(), Run)
      .WillByDefault(Return(ByMove(std::move(mock_dialog_))));
}

TEST_F(PasswordAccessoryControllerTest, IsNotRecreatedForSameWebContents) {
  PasswordAccessoryControllerImpl* initial_controller =
      PasswordAccessoryControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  PasswordAccessoryControllerImpl::CreateForWebContents(web_contents());
  EXPECT_EQ(PasswordAccessoryControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

TEST_F(PasswordAccessoryControllerTest, TransformsMatchesToSuggestions) {
  controller()->SavePasswordsForOrigin({CreateEntry("Ben", "S3cur3").first},
                                       url::Origin::Create(GURL(kExampleSite)));

  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestionsForField(
          /*is_fillable=*/true,
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           false)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/false);
}

TEST_F(PasswordAccessoryControllerTest, HintsToEmptyUserNames) {
  controller()->SavePasswordsForOrigin({CreateEntry("", "S3cur3").first},
                                       url::Origin::Create(GURL(kExampleSite)));

  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestionsForField(
          /*is_fillable=*/true,
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(no_user_str(), no_user_str(), false, false)
              .AppendField(ASCIIToUTF16("S3cur3"),
                           password_for_str(no_user_str()), true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/false);
}

TEST_F(PasswordAccessoryControllerTest, SortsAlphabeticalDuringTransform) {
  controller()->SavePasswordsForOrigin(
      {CreateEntry("Ben", "S3cur3").first, CreateEntry("Zebra", "M3h").first,
       CreateEntry("Alf", "PWD").first, CreateEntry("Cat", "M1@u").first},
      url::Origin::Create(GURL(kExampleSite)));

  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestionsForField(
          /*is_fillable=*/true,
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Alf"), ASCIIToUTF16("Alf"), false,
                           false)
              .AppendField(ASCIIToUTF16("PWD"), password_for_str("Alf"), true,
                           false)
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           false)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Cat"), ASCIIToUTF16("Cat"), false,
                           false)
              .AppendField(ASCIIToUTF16("M1@u"), password_for_str("Cat"), true,
                           false)
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Zebra"), ASCIIToUTF16("Zebra"), false,
                           false)
              .AppendField(ASCIIToUTF16("M3h"), password_for_str("Zebra"), true,
                           false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/false);
}

TEST_F(PasswordAccessoryControllerTest, RepeatsSuggestionsForSameFrame) {
  controller()->SavePasswordsForOrigin({CreateEntry("Ben", "S3cur3").first},
                                       url::Origin::Create(GURL(kExampleSite)));

  // Pretend that any input in the same frame was focused.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestionsForField(
          /*is_fillable=*/true,
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           false)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_fillable=*/false);
}

TEST_F(PasswordAccessoryControllerTest, ProvidesEmptySuggestionsMessage) {
  controller()->SavePasswordsForOrigin({},
                                       url::Origin::Create(GURL(kExampleSite)));

  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestionsForField(
                  /*is_fillable=*/true, PasswordAccessorySheetDataBuilder(
                                            passwords_empty_str(kExampleDomain))
                                            .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/false);
}

TEST_F(PasswordAccessoryControllerTest, OnFilledIntoFocusedField) {
  EXPECT_CALL(mock_manual_filling_controller_,
              OnFilledIntoFocusedField(FillingStatus::ERROR_NOT_ALLOWED));
  controller()->OnFilledIntoFocusedField(FillingStatus::ERROR_NOT_ALLOWED);

  EXPECT_CALL(mock_manual_filling_controller_,
              OnFilledIntoFocusedField(FillingStatus::ERROR_NO_VALID_FIELD));
  controller()->OnFilledIntoFocusedField(FillingStatus::ERROR_NO_VALID_FIELD);

  EXPECT_CALL(mock_manual_filling_controller_,
              OnFilledIntoFocusedField(FillingStatus::SUCCESS));
  controller()->OnFilledIntoFocusedField(FillingStatus::SUCCESS);
}

TEST_F(PasswordAccessoryControllerTest, RelaysAutomaticGenerationAvailable) {
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(true));
  controller()->OnAutomaticGenerationStatusChanged(
      true, GetTestGenerationUIData1(), nullptr);
}

TEST_F(PasswordAccessoryControllerTest, RelaysAutmaticGenerationUnavailable) {
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(false));
  controller()->OnAutomaticGenerationStatusChanged(false, base::nullopt,
                                                   nullptr);
}

// Tests that if AutomaticGenerationStatusChanged(true) is called for different
// password forms, the form and field signatures used for password generation
// are updated.
TEST_F(PasswordAccessoryControllerTest,
       UpdatesSignaturesForDifferentGenerationForms) {
  // Called twice for different forms.
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(true))
      .Times(2);
  controller()->OnAutomaticGenerationStatusChanged(
      true, GetTestGenerationUIData1(),
      mock_password_manager_driver_->AsWeakPtr());
  PasswordGenerationUIData new_ui_data = GetTestGenerationUIData2();
  controller()->OnAutomaticGenerationStatusChanged(
      true, new_ui_data, mock_password_manager_driver_->AsWeakPtr());

  autofill::FormSignature form_signature =
      autofill::CalculateFormSignature(new_ui_data.password_form.form_data);
  autofill::FieldSignature field_signature =
      autofill::CalculateFieldSignatureByNameAndType(
          new_ui_data.generation_element, "password");

  MockPasswordGenerationDialogView* raw_dialog_view = mock_dialog_.get();

  base::string16 generated_password = ASCIIToUTF16("t3stp@ssw0rd");
  EXPECT_CALL(mock_dialog_factory(), Run)
      .WillOnce(Return(ByMove(std::move(mock_dialog_))));
  EXPECT_CALL(*(mock_password_manager_driver_.get()),
              GetPasswordGenerationManager())
      .WillOnce(Return(mock_generation_manager_.get()));
  EXPECT_CALL(*(mock_generation_manager_.get()),
              GeneratePassword(_, form_signature, field_signature,
                               uint32_t(new_ui_data.max_length), _))
      .WillOnce(Return(generated_password));
  EXPECT_CALL(*raw_dialog_view, Show(generated_password));
  controller()->OnGenerationRequested();
}

TEST_F(PasswordAccessoryControllerTest, PasswordFieldChangesSuggestionType) {
  controller()->SavePasswordsForOrigin({CreateEntry("Ben", "S3cur3").first},
                                       url::Origin::Create(GURL(kExampleSite)));
  // Pretend a username field was focused. This should result in non-interactive
  // suggestion.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestionsForField(
          /*is_fillable=*/true,
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           false)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/false);

  // Pretend that we focus a password field now: By triggering a refresh with
  // |is_password_field| set to true, all suggestions should become interactive.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestionsForField(
          /*is_fillable=*/true,
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           false)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, true)
              .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/true);
}

TEST_F(PasswordAccessoryControllerTest, CachesIsReplacedByNewPasswords) {
  controller()->SavePasswordsForOrigin({CreateEntry("Ben", "S3cur3").first},
                                       url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestionsForField(
          /*is_fillable=*/true,
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           false)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/false);

  controller()->SavePasswordsForOrigin({CreateEntry("Alf", "M3lm4k").first},
                                       url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestionsForField(
          /*is_fillable=*/true,
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Alf"), ASCIIToUTF16("Alf"), false,
                           false)
              .AppendField(ASCIIToUTF16("M3lm4k"), password_for_str("Alf"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/false);
}

TEST_F(PasswordAccessoryControllerTest, UnfillableFieldClearsSuggestions) {
  controller()->SavePasswordsForOrigin({CreateEntry("Ben", "S3cur3").first},
                                       url::Origin::Create(GURL(kExampleSite)));
  // Pretend a username field was focused. This should result in non-emtpy
  // suggestions.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestionsForField(
          /*is_fillable=*/true,
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           false)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/false);

  // Pretend that the focus was lost or moved to an unfillable field. Now, only
  // the empty state message should be sent.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestionsForField(
          /*is_fillable=*/false,
          PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
              .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/false,
      /*is_password_field=*/false);  // Unused.
}

TEST_F(PasswordAccessoryControllerTest, NavigatingMainFrameClearsSuggestions) {
  // Set any, non-empty password list and pretend a username field was focused.
  // This should result in non-emtpy suggestions.
  controller()->SavePasswordsForOrigin({CreateEntry("Ben", "S3cur3").first},
                                       url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestionsForField(
          /*is_fillable=*/true,
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo()
              .AppendField(ASCIIToUTF16("Ben"), ASCIIToUTF16("Ben"), false,
                           false)
              .AppendField(ASCIIToUTF16("S3cur3"), password_for_str("Ben"),
                           true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/false);

  // Pretend that the focus was lost or moved to an unfillable field.
  NavigateAndCommit(GURL("https://random.other-site.org/"));
  controller()->DidNavigateMainFrame();

  // Now, only the empty state message should be sent.
  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestionsForField(
                  /*is_fillable=*/true,
                  PasswordAccessorySheetDataBuilder(
                      passwords_empty_str("random.other-site.org"))
                      .Build()));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL("https://random.other-site.org/")),
      /*is_fillable=*/true,
      /*is_password_field=*/false);  // Unused.
}

TEST_F(PasswordAccessoryControllerTest, FetchFaviconForCurrentUrl) {
  base::MockCallback<base::OnceCallback<void(const gfx::Image&)>> mock_callback;

  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestionsForField(/*is_fillable=*/true, _));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/false);

  EXPECT_CALL(*favicon_service(), GetRawFaviconForPageURL(GURL(kExampleSite), _,
                                                          kIconSize, _, _, _))
      .WillOnce(favicon::PostReply<6>(favicon_base::FaviconRawBitmapResult()));
  EXPECT_CALL(mock_callback, Run);
  controller()->GetFavicon(kIconSize, mock_callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAccessoryControllerTest, RequestsFaviconsOnceForOneOrigin) {
  base::MockCallback<base::OnceCallback<void(const gfx::Image&)>> mock_callback;

  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestionsForField(
                                                   /*is_fillable=*/true, _));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/false);

  EXPECT_CALL(*favicon_service(), GetRawFaviconForPageURL(GURL(kExampleSite), _,
                                                          kIconSize, _, _, _))
      .WillOnce(favicon::PostReply<6>(favicon_base::FaviconRawBitmapResult()));
  EXPECT_CALL(mock_callback, Run).Times(2);
  controller()->GetFavicon(kIconSize, mock_callback.Get());
  // The favicon service should already start to work on the request.
  Mock::VerifyAndClearExpectations(favicon_service());

  // This call is only enqueued (and the callback will be called afterwards).
  controller()->GetFavicon(kIconSize, mock_callback.Get());

  // After the async task is finished, both callbacks must be called.
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAccessoryControllerTest, FaviconsAreCachedUntilNavigation) {
  base::MockCallback<base::OnceCallback<void(const gfx::Image&)>> mock_callback;

  // We need a result with a non-empty image or it won't get cached.
  favicon_base::FaviconRawBitmapResult non_empty_result;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kIconSize, kIconSize);
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data->data());
  non_empty_result.bitmap_data = data;
  non_empty_result.expired = false;
  non_empty_result.pixel_size = gfx::Size(kIconSize, kIconSize);
  non_empty_result.icon_type = favicon_base::IconType::kFavicon;
  non_empty_result.icon_url = GURL(kExampleSite);

  // Populate the cache by requesting a favicon.
  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestionsForField(/*is_fillable=*/true, _));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)),
      /*is_fillable=*/true,
      /*is_password_field=*/false);

  EXPECT_CALL(*favicon_service(), GetRawFaviconForPageURL(GURL(kExampleSite), _,
                                                          kIconSize, _, _, _))
      .WillOnce(favicon::PostReply<6>(non_empty_result));
  EXPECT_CALL(mock_callback, Run).Times(1);
  controller()->GetFavicon(kIconSize, mock_callback.Get());

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&mock_callback);

  // This call is handled by the cache - no favicon service, no async request.
  EXPECT_CALL(mock_callback, Run).Times(1);
  controller()->GetFavicon(kIconSize, mock_callback.Get());
  Mock::VerifyAndClearExpectations(&mock_callback);
  Mock::VerifyAndClearExpectations(favicon_service());

  // The navigation to another origin clears the cache.
  NavigateAndCommit(GURL("https://random.other-site.org/"));
  controller()->DidNavigateMainFrame();
  NavigateAndCommit(GURL(kExampleSite));  // Same origin as intially.
  controller()->DidNavigateMainFrame();
  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestionsForField(/*is_fillable=*/true, _));
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)), /*is_fillable=*/true,
      /*is_password_field=*/false);

  // The cache was cleared, so now the service has to be queried again.
  EXPECT_CALL(*favicon_service(), GetRawFaviconForPageURL(GURL(kExampleSite), _,
                                                          kIconSize, _, _, _))
      .WillOnce(favicon::PostReply<6>(non_empty_result));
  EXPECT_CALL(mock_callback, Run).Times(1);
  controller()->GetFavicon(kIconSize, mock_callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAccessoryControllerTest, NoFaviconCallbacksWhenOriginChanges) {
  base::MockCallback<base::OnceCallback<void(const gfx::Image&)>> mock_callback;

  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestionsForField(/*is_fillable=*/true, _))
      .Times(2);
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL(kExampleSite)), true, false);

  // Right after starting the favicon request for example.com, another frame on
  // the same site is focused. Even if the request is completed, the callback
  // should not be called because the origin of the suggestions has changed.
  EXPECT_CALL(*favicon_service(), GetRawFaviconForPageURL(GURL(kExampleSite), _,
                                                          kIconSize, _, _, _))
      .WillOnce(favicon::PostReply<6>(favicon_base::FaviconRawBitmapResult()));
  EXPECT_CALL(mock_callback, Run).Times(0);
  controller()->GetFavicon(kIconSize, mock_callback.Get());
  controller()->RefreshSuggestionsForField(
      url::Origin::Create(GURL("https://other.frame.com/")), true, false);

  base::RunLoop().RunUntilIdle();
}

TEST_F(PasswordAccessoryControllerTest, RecordsGeneratedPasswordRejected) {
  base::string16 test_password = ASCIIToUTF16("t3stp@ssw0rd");

  InitializeGeneration(test_password);

  base::HistogramTester histogram_tester;

  controller()->OnGenerationRequested();
  controller()->GeneratedPasswordRejected();

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.GeneratedPasswordDialog", false, 1);
}

TEST_F(PasswordAccessoryControllerTest, RecordsGeneratedPasswordAccepted) {
  base::string16 test_password = ASCIIToUTF16("t3stp@ssw0rd");

  InitializeGeneration(test_password);

  base::HistogramTester histogram_tester;

  controller()->OnGenerationRequested();
  controller()->GeneratedPasswordAccepted(test_password);

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.GeneratedPasswordDialog", true, 1);
}

TEST_F(PasswordAccessoryControllerTest, Hide) {
  EXPECT_CALL(mock_manual_filling_controller_, Hide());
  controller()->Hide();
}
