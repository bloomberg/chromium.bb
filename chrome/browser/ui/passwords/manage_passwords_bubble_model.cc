// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using autofill::PasswordFormMap;
using content::WebContents;
namespace metrics_util = password_manager::metrics_util;

namespace {

enum FieldType { USERNAME_FIELD, PASSWORD_FIELD };

const int kUsernameFieldSize = 30;
const int kPasswordFieldSize = 22;

// Returns the width of |type| field.
int GetFieldWidth(FieldType type) {
  return ui::ResourceBundle::GetSharedInstance()
      .GetFontList(ui::ResourceBundle::SmallFont)
      .GetExpectedTextWidth(type == USERNAME_FIELD ? kUsernameFieldSize
                                                   : kPasswordFieldSize);
}

void SetupLinkifiedText(const base::string16& string_with_separator,
                        base::string16* text,
                        gfx::Range* link_range) {
  std::vector<base::string16> pieces;
  base::SplitStringDontTrim(string_with_separator,
                            '|',  // separator
                            &pieces);
  DCHECK_EQ(3u, pieces.size());
  *link_range = gfx::Range(pieces[0].size(),
                           pieces[0].size() + pieces[1].size());
  *text = JoinString(pieces, base::string16());
}

}  // namespace

ManagePasswordsBubbleModel::ManagePasswordsBubbleModel(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      display_disposition_(
          metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING),
      dismissal_reason_(metrics_util::NOT_DISPLAYED) {
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_contents);

  // TODO(mkwst): Reverse this logic. The controller should populate the model
  // directly rather than the model pulling from the controller. Perhaps like
  // `controller->PopulateModel(this)`.
  state_ = controller->state();
  if (password_manager::ui::IsPendingState(state_))
    pending_credentials_ = controller->PendingCredentials();
  best_matches_ = controller->best_matches();

  if (password_manager::ui::IsPendingState(state_)) {
    title_ = l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD);
  } else if (state_ == password_manager::ui::BLACKLIST_STATE) {
    title_ = l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_BLACKLISTED_TITLE);
  } else if (state_ == password_manager::ui::CONFIRMATION_STATE) {
    title_ =
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TITLE);
  } else {
    title_ = l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_TITLE);
  }

  SetupLinkifiedText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TEXT),
      &save_confirmation_text_,
      &save_confirmation_link_range_);

  manage_link_ =
      l10n_util::GetStringUTF16(IDS_OPTIONS_PASSWORDS_MANAGE_PASSWORDS_LINK);
}

ManagePasswordsBubbleModel::~ManagePasswordsBubbleModel() {}

void ManagePasswordsBubbleModel::OnBubbleShown(
    ManagePasswordsBubble::DisplayReason reason) {
  if (reason == ManagePasswordsBubble::USER_ACTION) {
    if (password_manager::ui::IsPendingState(state_)) {
      display_disposition_ = metrics_util::MANUAL_WITH_PASSWORD_PENDING;
    } else if (state_ == password_manager::ui::BLACKLIST_STATE) {
      display_disposition_ = metrics_util::MANUAL_BLACKLISTED;
    } else {
      display_disposition_ = metrics_util::MANUAL_MANAGE_PASSWORDS;
    }
  } else {
    if (state_ == password_manager::ui::CONFIRMATION_STATE) {
      display_disposition_ =
          metrics_util::AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION;
    } else {
      display_disposition_ = metrics_util::AUTOMATIC_WITH_PASSWORD_PENDING;
    }
  }
  metrics_util::LogUIDisplayDisposition(display_disposition_);

  // Default to a dismissal reason of "no interaction". If the user interacts
  // with the button in such a way that it closes, we'll reset this value
  // accordingly.
  dismissal_reason_ = metrics_util::NO_DIRECT_INTERACTION;
}

void ManagePasswordsBubbleModel::OnBubbleHidden() {
  if (dismissal_reason_ == metrics_util::NOT_DISPLAYED)
    return;

  metrics_util::LogUIDismissalReason(dismissal_reason_);
}

void ManagePasswordsBubbleModel::OnNopeClicked() {
  dismissal_reason_ = metrics_util::CLICKED_NOPE;
  state_ = password_manager::ui::PENDING_PASSWORD_STATE;
}

void ManagePasswordsBubbleModel::OnNeverForThisSiteClicked() {
  dismissal_reason_ = metrics_util::CLICKED_NEVER;
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->NeverSavePassword();
  state_ = password_manager::ui::BLACKLIST_STATE;
}

void ManagePasswordsBubbleModel::OnUnblacklistClicked() {
  dismissal_reason_ = metrics_util::CLICKED_UNBLACKLIST;
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->UnblacklistSite();
  state_ = password_manager::ui::MANAGE_STATE;
}

void ManagePasswordsBubbleModel::OnSaveClicked() {
  dismissal_reason_ = metrics_util::CLICKED_SAVE;
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  manage_passwords_ui_controller->SavePassword();
  state_ = password_manager::ui::MANAGE_STATE;
}

void ManagePasswordsBubbleModel::OnDoneClicked() {
  dismissal_reason_ = metrics_util::CLICKED_DONE;
}

// TODO(gcasto): Is it worth having this be separate from OnDoneClicked()?
// User intent is pretty similar in both cases.
void ManagePasswordsBubbleModel::OnOKClicked() {
  dismissal_reason_ = metrics_util::CLICKED_OK;
}

void ManagePasswordsBubbleModel::OnManageLinkClicked() {
  dismissal_reason_ = metrics_util::CLICKED_MANAGE;
  ManagePasswordsUIController::FromWebContents(web_contents())
      ->NavigateToPasswordManagerSettingsPage();
}

void ManagePasswordsBubbleModel::OnPasswordAction(
    const autofill::PasswordForm& password_form,
    PasswordAction action) {
  if (!web_contents())
    return;
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  password_manager::PasswordStore* password_store =
      PasswordStoreFactory::GetForProfile(profile, Profile::EXPLICIT_ACCESS)
          .get();
  DCHECK(password_store);
  if (action == REMOVE_PASSWORD)
    password_store->RemoveLogin(password_form);
  else
    password_store->AddLogin(password_form);
}

// static
int ManagePasswordsBubbleModel::UsernameFieldWidth() {
  return GetFieldWidth(USERNAME_FIELD);
}

// static
int ManagePasswordsBubbleModel::PasswordFieldWidth() {
  return GetFieldWidth(PASSWORD_FIELD);
}
