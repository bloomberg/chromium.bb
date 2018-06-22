// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_accessory_controller.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_accessory_view_interface.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#include "chrome/browser/android/preferences/preferences_launcher.h"

using autofill::PasswordForm;
using Item = PasswordAccessoryViewInterface::AccessoryItem;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PasswordAccessoryController);

PasswordAccessoryController::PasswordAccessoryController(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      view_(PasswordAccessoryViewInterface::Create(this)) {}

// Additional creation functions in unit tests only:
PasswordAccessoryController::PasswordAccessoryController(
    content::WebContents* web_contents,
    std::unique_ptr<PasswordAccessoryViewInterface> view)
    : web_contents_(web_contents), view_(std::move(view)) {}

PasswordAccessoryController::~PasswordAccessoryController() = default;

// static
void PasswordAccessoryController::CreateForWebContentsForTesting(
    content::WebContents* web_contents,
    std::unique_ptr<PasswordAccessoryViewInterface> view) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(!FromWebContents(web_contents)) << "Controller already attached!";
  web_contents->SetUserData(UserDataKey(),
                            base::WrapUnique(new PasswordAccessoryController(
                                web_contents, std::move(view))));
}

void PasswordAccessoryController::OnPasswordsAvailable(
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const GURL& origin) {
  const url::Origin& tab_origin =
      web_contents_->GetMainFrame()->GetLastCommittedOrigin();
  if (!tab_origin.IsSameOriginWith(url::Origin::Create(origin))) {
    // TODO(fhorschig): Support iframes: https://crbug.com/854150.
    return;
  }
  DCHECK(view_);
  std::vector<Item> items;
  base::string16 passwords_title_str;
  passwords_title_str = l10n_util::GetStringFUTF16(
      best_matches.empty()
          ? IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE
          : IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE,
      base::ASCIIToUTF16(origin.host()));
  items.emplace_back(passwords_title_str, passwords_title_str,
                     /*is_password=*/false, Item::Type::LABEL);
  for (const auto& pair : best_matches) {
    const PasswordForm* form = pair.second;
    base::string16 username = GetDisplayUsername(*form);
    items.emplace_back(username, username, /*is_password=*/false,
                       form->username_value.empty()
                           ? Item::Type::NON_INTERACTIVE_SUGGESTION
                           : Item::Type::SUGGESTION);
    items.emplace_back(
        form->password_value,
        l10n_util::GetStringFUTF16(
            IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, username),
        /*is_password=*/true, Item::Type::SUGGESTION);
  }
  items.emplace_back(base::string16(), base::string16(), false,
                     Item::Type::DIVIDER);
  base::string16 manage_passwords_title = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK);
  items.emplace_back(manage_passwords_title, manage_passwords_title, false,
                     Item::Type::OPTION);
  view_->OnItemsAvailable(origin, items);
}

void PasswordAccessoryController::DidNavigateMainFrame() {
  // Sending no passwords removes stale stuggestions and sends default options.
  OnPasswordsAvailable(
      /*best_matches=*/{},
      web_contents_->GetMainFrame()->GetLastCommittedOrigin().GetURL());
}

void PasswordAccessoryController::OnFillingTriggered(
    bool is_password,
    const base::string16& textToFill) const {
  password_manager::ContentPasswordManagerDriverFactory* factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          web_contents_);
  DCHECK(factory);
  // TODO(fhorschig): Consider allowing filling on non-main frames.
  password_manager::ContentPasswordManagerDriver* driver =
      factory->GetDriverForFrame(web_contents_->GetMainFrame());
  if (!driver) {
    return;
  }  // |driver| can be NULL if the tab is being closed.
  driver->FillIntoFocusedField(is_password, textToFill);
}

void PasswordAccessoryController::OnOptionSelected(
    const base::string16& selectedOption) const {
  if (selectedOption ==
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK)) {
    chrome::android::PreferencesLauncher::ShowPasswordSettings();
  }
}

gfx::NativeView PasswordAccessoryController::container_view() const {
  return web_contents_->GetNativeView();
}
