// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include "ash/login/ui/login_constants.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/size_range_layout.h"
#include "ash/system/user/button_from_view.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

// TODO(jdufault): On two user view the password prompt is visible to
// accessibility using special navigation keys even though it is invisible. We
// probably need to customize the text box quite a bit, we may want to do
// something similar to SearchBoxView.

namespace ash {
namespace {

// Size (width/height) of the submit button.
constexpr int kSubmitButtonSizeDp = 20;

// Width/height of the password inputfield.
constexpr int kPasswordInputWidthDp = 184;
constexpr int kPasswordInputHeightDp = 36;

// Total width of the password view.
constexpr int kPasswordTotalWidthDp = 204;

// Distance between the last password dot and the submit arrow/button.
constexpr int kDistanceBetweenPasswordAndSubmitDp = 0;

// Color of the password field text.
constexpr SkColor kTextColor = SkColorSetARGBMacro(0xAB, 0xFF, 0xFF, 0xFF);

constexpr const char kLoginPasswordViewName[] = "LoginPasswordView";

class NonAccessibleSeparator : public views::Separator {
 public:
  NonAccessibleSeparator() = default;
  ~NonAccessibleSeparator() override = default;

  // views::Separator:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    views::Separator::GetAccessibleNodeData(node_data);
    node_data->AddState(ui::AX_STATE_INVISIBLE);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NonAccessibleSeparator);
};

}  // namespace

LoginPasswordView::TestApi::TestApi(LoginPasswordView* view) : view_(view) {}

LoginPasswordView::TestApi::~TestApi() = default;

views::View* LoginPasswordView::TestApi::textfield() const {
  return view_->textfield_;
}

views::View* LoginPasswordView::TestApi::submit_button() const {
  return view_->submit_button_;
}

LoginPasswordView::LoginPasswordView() {
  auto* root_layout = new views::BoxLayout(views::BoxLayout::kVertical);
  root_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(root_layout);

  auto* row = new NonAccessibleView();
  AddChildView(row);
  auto* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, gfx::Insets(),
                           kDistanceBetweenPasswordAndSubmitDp);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  row->SetLayoutManager(layout);

  // Password textfield. We control the textfield size by sizing the parent
  // view, as the textfield will expand to fill it.
  auto* textfield_sizer = new NonAccessibleView();
  textfield_sizer->SetLayoutManager(new SizeRangeLayout(
      gfx::Size(kPasswordInputWidthDp, kPasswordInputHeightDp)));
  textfield_ = new views::Textfield();
  textfield_->set_controller(this);
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  textfield_->SetTextColor(kTextColor);
  textfield_->set_placeholder_text_color(kTextColor);

  textfield_->SetBorder(nullptr);
  textfield_->SetBackgroundColor(SK_ColorTRANSPARENT);

  textfield_sizer->AddChildView(textfield_);
  row->AddChildView(textfield_sizer);

  // Submit button.
  submit_button_ = new views::ImageButton(this);
  submit_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kLockScreenArrowIcon, kSubmitButtonSizeDp,
                            login_constants::kButtonEnabledColor));
  submit_button_->SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(
          kLockScreenArrowIcon, kSubmitButtonSizeDp,
          SkColorSetA(login_constants::kButtonEnabledColor,
                      login_constants::kButtonDisabledAlpha)));
  submit_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                    views::ImageButton::ALIGN_MIDDLE);
  submit_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_POD_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  row->AddChildView(submit_button_);

  // Separator on bottom.
  separator_ = new NonAccessibleSeparator();
  AddChildView(separator_);

  // Make sure the textfield always starts with focus.
  textfield_->RequestFocus();
}

LoginPasswordView::~LoginPasswordView() = default;

void LoginPasswordView::Init(
    const OnPasswordSubmit& on_submit,
    const OnPasswordTextChanged& on_password_text_changed) {
  DCHECK(on_submit);
  DCHECK(on_password_text_changed);
  on_submit_ = on_submit;
  on_password_text_changed_ = on_password_text_changed;
}

void LoginPasswordView::UpdateForUser(const mojom::UserInfoPtr& user) {
  textfield_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME,
      base::UTF8ToUTF16(user->display_email)));
}

void LoginPasswordView::SetFocusEnabledForChildViews(bool enable) {
  auto behavior = enable ? FocusBehavior::ALWAYS : FocusBehavior::NEVER;
  textfield_->SetFocusBehavior(behavior);
}

void LoginPasswordView::Clear() {
  textfield_->SetText(base::string16());
  // |ContentsChanged| won't be called by |Textfield| if the text is changed
  // by |Textfield::SetText()|.
  ContentsChanged(textfield_, textfield_->text());
}

void LoginPasswordView::AppendNumber(int value) {
  textfield_->SetText(textfield_->text() + base::IntToString16(value));
  // |ContentsChanged| won't be called by |Textfield| if the text is changed
  // by |Textfield::AppendText()|.
  ContentsChanged(textfield_, textfield_->text());
}

void LoginPasswordView::Backspace() {
  // Instead of just adjusting textfield_ text directly, fire a backspace key
  // event as this handles the various edge cases (ie, selected text).

  // views::Textfield::OnKeyPressed is private, so we call it via views::View.
  auto* view = static_cast<views::View*>(textfield_);
  view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
  view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
}

void LoginPasswordView::Submit() {}

void LoginPasswordView::SetPlaceholderText(
    const base::string16& placeholder_text) {
  textfield_->set_placeholder_text(placeholder_text);
}

const char* LoginPasswordView::GetClassName() const {
  return kLoginPasswordViewName;
}

gfx::Size LoginPasswordView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  size.set_width(kPasswordTotalWidthDp);
  return size;
}

void LoginPasswordView::RequestFocus() {
  textfield_->RequestFocus();
}

bool LoginPasswordView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::KeyboardCode::VKEY_RETURN) {
    SubmitPassword();
    return true;
  }

  return false;
}

void LoginPasswordView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  DCHECK_EQ(sender, submit_button_);
  SubmitPassword();
}

void LoginPasswordView::ContentsChanged(views::Textfield* sender,
                                        const base::string16& new_contents) {
  DCHECK_EQ(sender, textfield_);
  bool is_enabled = !new_contents.empty();
  submit_button_->SetEnabled(is_enabled);
  on_password_text_changed_.Run(!is_enabled);
  separator_->SetColor(
      is_enabled ? login_constants::kButtonEnabledColor
                 : SkColorSetA(login_constants::kButtonEnabledColor,
                               login_constants::kButtonDisabledAlpha));
}

void LoginPasswordView::SubmitPassword() {
  on_submit_.Run(textfield_->text());
  Clear();
}

}  // namespace ash
