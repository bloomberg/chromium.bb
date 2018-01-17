// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_password_pending_view.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/manage_password_items_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_sign_in_promo_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/views/desktop_ios_promotion/desktop_ios_promotion_bubble_view.h"
#endif

namespace {

// TODO(pbos): Investigate expicitly obfuscating items inside ComboboxModel.
constexpr base::char16 kBulletChar = gfx::RenderText::kPasswordReplacementChar;

// A combobox model for password dropdown that allows to reveal/mask values in
// the combobox.
class PasswordDropdownModel : public ui::ComboboxModel {
 public:
  PasswordDropdownModel(bool revealed, const std::vector<base::string16>& items)
      : revealed_(revealed), passwords_(items) {}
  ~PasswordDropdownModel() override {}

  void SetRevealed(bool revealed) {
    if (revealed_ == revealed)
      return;
    revealed_ = revealed;
    for (auto& observer : observers_)
      observer.OnComboboxModelChanged(this);
  }

  // ui::ComboboxModel:
  int GetItemCount() const override { return passwords_.size(); }
  base::string16 GetItemAt(int index) override {
    return revealed_ ? passwords_[index]
                     : base::string16(passwords_[index].length(), kBulletChar);
  }
  void AddObserver(ui::ComboboxModelObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(ui::ComboboxModelObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

 private:
  bool revealed_;
  const std::vector<base::string16> passwords_;
  // To be called when |masked_| was changed;
  base::ObserverList<ui::ComboboxModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(PasswordDropdownModel);
};

std::unique_ptr<views::ToggleImageButton> CreatePasswordViewButton(
    views::ButtonListener* listener,
    bool are_passwords_revealed) {
  std::unique_ptr<views::ToggleImageButton> button(
      new views::ToggleImageButton(listener));
  button->SetFocusForPlatform();
  button->set_request_focus_on_press(true);
  button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_SHOW_PASSWORD));
  button->SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_HIDE_PASSWORD));
  button->SetImage(views::ImageButton::STATE_NORMAL,
                   *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                       IDR_SHOW_PASSWORD_HOVER));
  button->SetToggledImage(
      views::ImageButton::STATE_NORMAL,
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_HIDE_PASSWORD_HOVER));
  button->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                            views::ImageButton::ALIGN_MIDDLE);
  button->SetToggled(are_passwords_revealed);
  return button;
}

// Creates a dropdown from |PasswordForm.all_possible_passwords|.
std::unique_ptr<views::Combobox> CreatePasswordDropdownView(
    const autofill::PasswordForm& form,
    bool are_passwords_revealed) {
  DCHECK(!form.all_possible_passwords.empty());
  std::unique_ptr<views::Combobox> combobox =
      std::make_unique<views::Combobox>(std::make_unique<PasswordDropdownModel>(
          are_passwords_revealed, form.all_possible_passwords));
  size_t index = std::distance(
      form.all_possible_passwords.begin(),
      find(form.all_possible_passwords.begin(),
           form.all_possible_passwords.end(), form.password_value));
  // Unlikely, but if we don't find the password in possible passwords,
  // we will set the default to first element.
  if (index == form.all_possible_passwords.size()) {
    NOTREACHED();
    combobox->SetSelectedIndex(0);
  } else {
    combobox->SetSelectedIndex(index);
  }
  combobox->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_PASSWORD_LABEL));
  return combobox;
}
}  // namespace

ManagePasswordPendingView::ManagePasswordPendingView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent),
      save_button_(nullptr),
      never_button_(nullptr),
      username_field_(nullptr),
      password_view_button_(nullptr),
      password_dropdown_(nullptr),
      password_label_(nullptr),
      are_passwords_revealed_(
          parent_->model()->are_passwords_revealed_when_bubble_is_opened()) {
  // Create credentials row.
  const autofill::PasswordForm& password_form =
      parent_->model()->pending_password();
  const bool is_password_credential = password_form.federation_origin.unique();
  if (parent_->model()->enable_editing()) {
    username_field_ = CreateUsernameEditable(password_form).release();
  } else {
    username_field_ = CreateUsernameLabel(password_form).release();
  }

  CreatePasswordField();

  if (is_password_credential) {
    password_view_button_ =
        CreatePasswordViewButton(this, are_passwords_revealed_).release();
  }

  // Create buttons.
  save_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON));
  never_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BUBBLE_BLACKLIST_BUTTON));

  CreateAndSetLayout(is_password_credential);
  if (parent_->model()->enable_editing() &&
      parent_->model()->pending_password().username_value.empty()) {
    parent_->set_initially_focused_view(username_field_);
  } else {
    parent_->set_initially_focused_view(save_button_);
  }
}

ManagePasswordPendingView::~ManagePasswordPendingView() = default;

void ManagePasswordPendingView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  if (sender == save_button_) {
    UpdateUsernameAndPasswordInModel();
    parent_->model()->OnSaveClicked();
    if (parent_->model()->ReplaceToShowPromotionIfNeeded()) {
      ReplaceWithPromo();
      return;
    }
  } else if (sender == never_button_) {
    parent_->model()->OnNeverForThisSiteClicked();
  } else if (sender == password_view_button_) {
    TogglePasswordVisibility();
    return;
  } else {
    NOTREACHED();
  }

  parent_->CloseBubble();
}

gfx::Size ManagePasswordPendingView::CalculatePreferredSize() const {
  return gfx::Size(ManagePasswordsBubbleView::kDesiredBubbleWidth,
                   GetLayoutManager()->GetPreferredHeightForWidth(
                       this, ManagePasswordsBubbleView::kDesiredBubbleWidth));
}

void ManagePasswordPendingView::CreateAndSetLayout(bool show_password_label) {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  layout->set_minimum_size(
      gfx::Size(ManagePasswordsBubbleView::kDesiredBubbleWidth, 0));

  views::View* password_field =
      password_dropdown_ ? static_cast<views::View*>(password_dropdown_)
                         : static_cast<views::View*>(password_label_);
  ManagePasswordsBubbleView::BuildCredentialRows(
      layout, username_field_, password_field, password_view_button_,
      show_password_label);
  layout->AddPaddingRow(
      0, ChromeLayoutProvider::Get()->GetDistanceMetric(
             views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL));

  // Button row.
  ManagePasswordsBubbleView::BuildColumnSet(
      layout, ManagePasswordsBubbleView::DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRow(0, ManagePasswordsBubbleView::DOUBLE_BUTTON_COLUMN_SET);
  layout->AddView(save_button_);
  layout->AddView(never_button_);
}

void ManagePasswordPendingView::CreatePasswordField() {
  const autofill::PasswordForm& password_form =
      parent_->model()->pending_password();
  if (password_form.all_possible_passwords.size() > 1 &&
      parent_->model()->enable_editing()) {
    password_dropdown_ =
        CreatePasswordDropdownView(password_form, are_passwords_revealed_)
            .release();
  } else {
    password_label_ =
        CreatePasswordLabel(password_form,
                            IDS_PASSWORD_MANAGER_SIGNIN_VIA_FEDERATION,
                            are_passwords_revealed_)
            .release();
  }
}

void ManagePasswordPendingView::TogglePasswordVisibility() {
  if (!are_passwords_revealed_ && !parent_->model()->RevealPasswords())
    return;

  UpdateUsernameAndPasswordInModel();
  are_passwords_revealed_ = !are_passwords_revealed_;
  password_view_button_->SetToggled(are_passwords_revealed_);
  DCHECK(!password_dropdown_ || !password_label_);
  if (password_dropdown_) {
    static_cast<PasswordDropdownModel*>(password_dropdown_->model())
        ->SetRevealed(are_passwords_revealed_);
  } else {
    password_label_->SetObscured(!are_passwords_revealed_);
  }
}

void ManagePasswordPendingView::UpdateUsernameAndPasswordInModel() {
  const bool username_editable = parent_->model()->enable_editing();
  const bool password_editable =
      password_dropdown_ && parent_->model()->enable_editing();
  if (!username_editable && !password_editable)
    return;

  base::string16 new_username =
      parent_->model()->pending_password().username_value;
  base::string16 new_password =
      parent_->model()->pending_password().password_value;
  if (username_editable) {
    new_username = static_cast<views::Textfield*>(username_field_)->text();
    base::TrimString(new_username, base::ASCIIToUTF16(" "), &new_username);
  }
  if (password_editable) {
    new_password =
        parent_->model()->pending_password().all_possible_passwords.at(
            password_dropdown_->selected_index());
  }
  parent_->model()->OnCredentialEdited(new_username, new_password);
}

void ManagePasswordPendingView::ReplaceWithPromo() {
  RemoveAllChildViews(true);
  parent_->initially_focused_view_ = NULL;
  SetLayoutManager(std::make_unique<views::FillLayout>());
  if (parent_->model()->state() ==
      password_manager::ui::CHROME_SIGN_IN_PROMO_STATE) {
    AddChildView(new ManagePasswordSignInPromoView(parent_));
#if defined(OS_WIN)
  } else if (parent_->model()->state() ==
             password_manager::ui::CHROME_DESKTOP_IOS_PROMO_STATE) {
    AddChildView(new DesktopIOSPromotionBubbleView(
        parent_->model()->GetProfile(),
        desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE));
#endif
  } else {
    NOTREACHED();
  }
  parent_->GetWidget()->UpdateWindowIcon();
  parent_->UpdateTitleText(
      static_cast<views::StyledLabel*>(parent_->GetBubbleFrameView()->title()));
  parent_->DialogModelChanged();
  parent_->SizeToContents();
}
