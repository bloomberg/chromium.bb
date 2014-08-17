// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/accounts_detailed_view.h"

#include <vector>

#include "ash/multi_profile_uma.h"
#include "ash/shell.h"
#include "ash/system/tray/fixed_sized_scroll_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_header_button.h"
#include "ash/system/user/config.h"
#include "ash/system/user/tray_user.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user_info.h"
#include "grit/ash_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {
namespace tray {

namespace {

const int kAccountsViewVerticalPadding = 12;
const int kPrimaryAccountColumnSetID = 0;
const int kSecondaryAccountColumnSetID = 1;
const int kPaddingBetweenAccounts = 20;

}  // namespace

AccountsDetailedView::AccountsDetailedView(TrayUser* owner,
                                           user::LoginStatus login_status)
    : TrayDetailsView(owner),
      delegate_(NULL),
      account_list_(NULL),
      add_account_button_(NULL),
      add_user_button_(NULL) {
  std::string user_id = Shell::GetInstance()
                            ->session_state_delegate()
                            ->GetUserInfo(0)
                            ->GetUserID();
  delegate_ =
      Shell::GetInstance()->system_tray_delegate()->GetUserAccountsDelegate(
          user_id);
  delegate_->AddObserver(this);
  AddHeader(login_status);
  CreateScrollableList();
  AddAccountList();
  AddAddAccountButton();
  AddFooter();
}

AccountsDetailedView::~AccountsDetailedView() {
  delegate_->RemoveObserver(this);
}

void AccountsDetailedView::OnViewClicked(views::View* sender) {
  if (sender == footer()->content())
    TransitionToDefaultView();
  else if (sender == add_account_button_)
    delegate_->LaunchAddAccountDialog();
  else
    NOTREACHED();
}

void AccountsDetailedView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  std::map<views::View*, std::string>::iterator it =
      delete_button_to_account_id_.find(sender);
  if (it != delete_button_to_account_id_.end()) {
    delegate_->DeleteAccount(it->second);
  } else if (add_user_button_ && add_user_button_ == sender) {
    MultiProfileUMA::RecordSigninUser(MultiProfileUMA::SIGNIN_USER_BY_TRAY);
    Shell::GetInstance()->system_tray_delegate()->ShowUserLogin();
    owner()->system_tray()->CloseSystemBubble();
  } else {
    NOTREACHED();
  }
}

void AccountsDetailedView::AccountListChanged() { UpdateAccountList(); }

void AccountsDetailedView::AddHeader(user::LoginStatus login_status) {
  views::View* user_view_container = new views::View;
  user_view_container->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  user_view_container->SetBorder(
      views::Border::CreateSolidSidedBorder(0, 0, 1, 0, kBorderLightColor));
  user_view_container->AddChildView(
      new tray::UserView(owner(), login_status, 0, true));
  AddChildView(user_view_container);
}

void AccountsDetailedView::AddAccountList() {
  scroll_content()->SetBorder(
      views::Border::CreateEmptyBorder(kAccountsViewVerticalPadding,
                                       kTrayPopupPaddingHorizontal,
                                       kAccountsViewVerticalPadding,
                                       kTrayPopupPaddingHorizontal));
  views::Label* account_list_title = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ACCOUNT_LIST_TITLE));
  account_list_title->SetEnabledColor(SkColorSetARGB(0x7f, 0, 0, 0));
  account_list_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  scroll_content()->AddChildView(account_list_title);
  account_list_ = new views::View();
  UpdateAccountList();
  scroll_content()->AddChildView(account_list_);
}

void AccountsDetailedView::AddAddAccountButton() {
  SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();
  HoverHighlightView* add_account_button = new HoverHighlightView(this);
  const user_manager::UserInfo* user_info =
      session_state_delegate->GetUserInfo(0);
  base::string16 user_name = user_info->GetGivenName();
  if (user_name.empty())
    user_name = user_info->GetDisplayName();
  if (user_name.empty())
    user_name = base::ASCIIToUTF16(user_info->GetEmail());
  add_account_button->AddLabel(
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_ADD_ACCOUNT_LABEL,
                                 user_name),
      gfx::ALIGN_CENTER,
      gfx::Font::NORMAL);
  AddChildView(add_account_button);
  add_account_button_ = add_account_button;
}

void AccountsDetailedView::AddFooter() {
  CreateSpecialRow(IDS_ASH_STATUS_TRAY_ACCOUNTS_TITLE, this);
  if (!IsMultiProfileSupportedAndUserActive())
    return;
  TrayPopupHeaderButton* add_user_button =
      new TrayPopupHeaderButton(this,
                                IDR_AURA_UBER_TRAY_ADD_PROFILE,
                                IDR_AURA_UBER_TRAY_ADD_PROFILE,
                                IDR_AURA_UBER_TRAY_ADD_PROFILE_HOVER,
                                IDR_AURA_UBER_TRAY_ADD_PROFILE_HOVER,
                                IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT);
  add_user_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
  footer()->AddButton(add_user_button);
  add_user_button_ = add_user_button;
}

void AccountsDetailedView::UpdateAccountList() {
  // Clear existing view.
  delete_button_to_account_id_.clear();
  account_list_->RemoveAllChildViews(true);

  // Configuring layout manager.
  views::GridLayout* layout = new views::GridLayout(account_list_);
  account_list_->SetLayoutManager(layout);
  views::ColumnSet* primary_account_row =
      layout->AddColumnSet(kPrimaryAccountColumnSetID);
  primary_account_row->AddColumn(views::GridLayout::LEADING,
                                 views::GridLayout::CENTER,
                                 1.0,
                                 views::GridLayout::USE_PREF,
                                 0,
                                 0);
  views::ColumnSet* secondary_account_row =
      layout->AddColumnSet(kSecondaryAccountColumnSetID);
  secondary_account_row->AddColumn(views::GridLayout::FILL,
                                   views::GridLayout::CENTER,
                                   1.0,
                                   views::GridLayout::USE_PREF,
                                   0,
                                   0);
  secondary_account_row->AddPaddingColumn(0.0, kTrayPopupPaddingBetweenItems);
  secondary_account_row->AddColumn(views::GridLayout::FILL,
                                   views::GridLayout::CENTER,
                                   0.0,
                                   views::GridLayout::USE_PREF,
                                   0,
                                   0);

  // Adding primary account.
  layout->AddPaddingRow(0.0, kPaddingBetweenAccounts);
  layout->StartRow(0.0, kPrimaryAccountColumnSetID);
  const std::string& primary_account = delegate_->GetPrimaryAccountId();
  views::Label* primary_account_label =
      new views::Label(l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_PRIMARY_ACCOUNT_LABEL,
          base::ASCIIToUTF16(
              delegate_->GetAccountDisplayName(primary_account))));
  layout->AddView(primary_account_label);

  // Adding secondary accounts.
  const std::vector<std::string>& secondary_accounts =
      delegate_->GetSecondaryAccountIds();
  for (size_t i = 0; i < secondary_accounts.size(); ++i) {
    layout->AddPaddingRow(0.0, kPaddingBetweenAccounts);
    layout->StartRow(0.0, kSecondaryAccountColumnSetID);
    const std::string& account_id = secondary_accounts[i];
    views::Label* account_label = new views::Label(
        base::ASCIIToUTF16(delegate_->GetAccountDisplayName(account_id)));
    account_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(account_label);
    views::View* delete_button = CreateDeleteButton();
    delete_button_to_account_id_[delete_button] = account_id;
    layout->AddView(delete_button);
  }

  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
}

views::View* AccountsDetailedView::CreateDeleteButton() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  views::ImageButton* delete_button = new views::ImageButton(this);
  delete_button->SetImage(
      views::Button::STATE_NORMAL,
      rb.GetImageNamed(IDR_AURA_UBER_TRAY_REMOVE_ACCOUNT).ToImageSkia());
  delete_button->SetImage(
      views::Button::STATE_HOVERED,
      rb.GetImageNamed(IDR_AURA_UBER_TRAY_REMOVE_ACCOUNT_HOVER).ToImageSkia());
  delete_button->SetImage(
      views::Button::STATE_PRESSED,
      rb.GetImageNamed(IDR_AURA_UBER_TRAY_REMOVE_ACCOUNT_HOVER).ToImageSkia());
  return delete_button;
}

}  // namespace tray
}  // namespace ash
