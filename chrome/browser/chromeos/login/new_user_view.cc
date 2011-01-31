// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/new_user_view.h"

#include <signal.h>
#include <sys/types.h>

#include <algorithm>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/textfield_with_margin.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/chromeos/views/copy_background.h"
#include "gfx/font.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"
#include "views/widget/widget_gtk.h"

using views::View;

namespace {

const int kTextfieldWidth = 230;
const int kSplitterHeight = 1;
const int kTitlePad = 20;
const int kRowPad = 13;
const int kBottomPad = 33;
const int kLeftPad = 33;
const int kColumnPad = 7;
const int kLanguagesMenuHeight = 25;
const int kLanguagesMenuPad = 5;
const SkColor kLanguagesMenuTextColor = 0xFF999999;
const SkColor kErrorColor = 0xFF8F384F;
const SkColor kSplitterUp1Color = 0xFFD0D2D3;
const SkColor kSplitterUp2Color = 0xFFE1E3E4;
const SkColor kSplitterDown1Color = 0xFFE3E6E8;
const SkColor kSplitterDown2Color = 0xFFEAEDEE;
const char kDefaultDomain[] = "@gmail.com";

// Textfield that adds domain to the entered username if focus is lost and
// username doesn't have full domain.
class UsernameField : public chromeos::TextfieldWithMargin {
 public:
  explicit UsernameField(chromeos::NewUserView* controller)
      : controller_(controller) {}

  // views::Textfield overrides:
  virtual void WillLoseFocus() {
    string16 user_input;
    bool was_trim = TrimWhitespace(text(), TRIM_ALL, &user_input) != TRIM_NONE;
    if (!user_input.empty()) {
      std::string username = UTF16ToUTF8(user_input);

      if (username.find('@') == std::string::npos) {
        username += kDefaultDomain;
        SetText(UTF8ToUTF16(username));
        was_trim = false;
      }
    }

    if (was_trim)
      SetText(user_input);
  }

  // Overridden from views::View:
  virtual bool OnKeyPressed(const views::KeyEvent& e) {
    if (e.GetKeyCode() == ui::VKEY_LEFT) {
      return controller_->NavigateAway();
    }
    return false;
  }

 private:
  chromeos::NewUserView* controller_;
  DISALLOW_COPY_AND_ASSIGN(UsernameField);
};

}  // namespace

namespace chromeos {

NewUserView::NewUserView(Delegate* delegate,
                         bool need_border,
                         bool need_guest_link)
    : username_field_(NULL),
      password_field_(NULL),
      title_label_(NULL),
      title_hint_label_(NULL),
      splitter_up1_(NULL),
      splitter_up2_(NULL),
      splitter_down1_(NULL),
      splitter_down2_(NULL),
      sign_in_button_(NULL),
      create_account_link_(NULL),
      guest_link_(NULL),
      languages_menubutton_(NULL),
      accel_focus_pass_(views::Accelerator(ui::VKEY_P, false, false, true)),
      accel_focus_user_(views::Accelerator(ui::VKEY_U, false, false, true)),
      accel_login_off_the_record_(
          views::Accelerator(ui::VKEY_B, false, false, true)),
      accel_toggle_accessibility_(WizardAccessibilityHelper::GetAccelerator()),
      delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(focus_grabber_factory_(this)),
      focus_delayed_(false),
      login_in_process_(false),
      need_border_(need_border),
      need_guest_link_(false),
      need_create_account_(false),
      languages_menubutton_order_(-1),
      sign_in_button_order_(-1) {
  if (need_guest_link && UserCrosSettingsProvider::cached_allow_guest())
    need_guest_link_ = true;
}

NewUserView::~NewUserView() {
}

void NewUserView::Init() {
  if (need_border_) {
    // Use rounded rect background.
    set_border(CreateWizardBorder(&BorderDefinition::kUserBorder));
    views::Painter* painter = CreateWizardPainter(
        &BorderDefinition::kUserBorder);
    set_background(views::Background::CreateBackgroundPainter(true, painter));
  }

  // Set up fonts.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font = rb.GetFont(ResourceBundle::MediumBoldFont).DeriveFont(
      kLoginTitleFontDelta);
  gfx::Font title_hint_font = rb.GetFont(ResourceBundle::BoldFont);

  title_label_ = new views::Label();
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label_->SetFont(title_font);
  title_label_->SetMultiLine(true);
  AddChildView(title_label_);

  title_hint_label_ = new views::Label();
  title_hint_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_hint_label_->SetFont(title_hint_font);
  title_hint_label_->SetColor(SK_ColorGRAY);
  title_hint_label_->SetMultiLine(true);
  AddChildView(title_hint_label_);

  splitter_up1_ = CreateSplitter(kSplitterUp1Color);
  splitter_up2_ = CreateSplitter(kSplitterUp2Color);
  splitter_down1_ = CreateSplitter(kSplitterDown1Color);
  splitter_down2_ = CreateSplitter(kSplitterDown2Color);

  username_field_ = new UsernameField(this);
  username_field_->set_background(new CopyBackground(this));
  username_field_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_CHROMEOS_ACC_USERNAME_LABEL));
  AddChildView(username_field_);

  password_field_ = new TextfieldWithMargin(views::Textfield::STYLE_PASSWORD);
  password_field_->set_background(new CopyBackground(this));
  AddChildView(password_field_);

  language_switch_menu_.InitLanguageMenu();

  RecreatePeculiarControls();

  AddChildView(sign_in_button_);
  if (need_create_account_) {
    InitLink(&create_account_link_);
  }
  if (need_guest_link_) {
    InitLink(&guest_link_);
  }
  AddChildView(languages_menubutton_);

  // Set up accelerators.
  AddAccelerator(accel_focus_user_);
  AddAccelerator(accel_focus_pass_);
  AddAccelerator(accel_login_off_the_record_);
  AddAccelerator(accel_toggle_accessibility_);

  OnLocaleChanged();

  // Controller to handle events from textfields
  username_field_->SetController(this);
  password_field_->SetController(this);
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    EnableInputControls(false);
  }

  // The 'Sign in' button should be disabled when there is no text in the
  // username and password fields.
  sign_in_button_->SetEnabled(false);
}

bool NewUserView::AcceleratorPressed(const views::Accelerator& accelerator) {
  if (accelerator == accel_focus_user_) {
    username_field_->RequestFocus();
  } else if (accelerator == accel_focus_pass_) {
    password_field_->RequestFocus();
  } else if (accelerator == accel_login_off_the_record_) {
    delegate_->OnLoginAsGuest();
  } else if (accelerator == accel_toggle_accessibility_) {
    WizardAccessibilityHelper::GetInstance()->ToggleAccessibility(this);
  } else {
    return false;
  }
  return true;
}

void NewUserView::RecreatePeculiarControls() {
  // PreferredSize reported by MenuButton (and TextField) is not able
  // to shrink, only grow; so recreate on text change.
  delete languages_menubutton_;
  languages_menubutton_ = new views::MenuButton(
      NULL, std::wstring(), &language_switch_menu_, true);
  languages_menubutton_->set_menu_marker(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_MENU_DROPARROW_SHARP));
  languages_menubutton_->SetEnabledColor(kLanguagesMenuTextColor);
  languages_menubutton_->SetFocusable(true);

  // There is no way to get native button preferred size after the button was
  // sized so delete and recreate the button on text update.
  delete sign_in_button_;
  sign_in_button_ = new login::WideButton(this, std::wstring());
  UpdateSignInButtonState();

  if (!CrosLibrary::Get()->EnsureLoaded())
    sign_in_button_->SetEnabled(false);
}

void NewUserView::UpdateSignInButtonState() {
  bool enabled = !username_field_->text().empty() &&
                 !password_field_->text().empty();
  sign_in_button_->SetEnabled(enabled);
}

views::View* NewUserView::CreateSplitter(SkColor color) {
  views::View* splitter = new views::View();
  splitter->set_background(views::Background::CreateSolidBackground(color));
  AddChildView(splitter);
  return splitter;
}

void NewUserView::AddChildView(View* view) {
  // languages_menubutton_ and sign_in_button_ are recreated on text change,
  // so we restore their original position in layout.
  if (view == languages_menubutton_) {
    if (languages_menubutton_order_ < 0) {
      languages_menubutton_order_ = GetChildViewCount();
    }
    views::View::AddChildView(languages_menubutton_order_, view);
  } else if (view == sign_in_button_) {
    if (sign_in_button_order_ < 0) {
      sign_in_button_order_ = GetChildViewCount();
    }
    views::View::AddChildView(sign_in_button_order_, view);
  } else {
    views::View::AddChildView(view);
  }
}

void NewUserView::UpdateLocalizedStrings() {
  title_label_->SetText(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_LOGIN_TITLE)));
  title_hint_label_->SetText(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_LOGIN_TITLE_HINT)));
  username_field_->set_text_to_display_when_empty(
      l10n_util::GetStringUTF16(IDS_LOGIN_USERNAME));
  password_field_->set_text_to_display_when_empty(
      l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD));
  sign_in_button_->SetLabel(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_LOGIN_BUTTON)));
  if (need_create_account_) {
    create_account_link_->SetText(
        UTF16ToWide(l10n_util::GetStringUTF16(IDS_CREATE_ACCOUNT_BUTTON)));
  }
  if (need_guest_link_) {
    guest_link_->SetText(UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_BROWSE_WITHOUT_SIGNING_IN_BUTTON)));
  }
  delegate_->ClearErrors();
  languages_menubutton_->SetText(
      UTF16ToWide(language_switch_menu_.GetCurrentLocaleName()));
}

void NewUserView::OnLocaleChanged() {
  RecreatePeculiarControls();
  UpdateLocalizedStrings();
  AddChildView(sign_in_button_);
  AddChildView(languages_menubutton_);

  Layout();
  SchedulePaint();
  RequestFocus();
}

void NewUserView::RequestFocus() {
  MessageLoop::current()->PostTask(FROM_HERE,
      focus_grabber_factory_.NewRunnableMethod(
          &NewUserView::FocusFirstField));
}

void NewUserView::ViewHierarchyChanged(bool is_add,
                                       View *parent,
                                       View *child) {
  if (is_add && child == this) {
    MessageLoop::current()->PostTask(FROM_HERE,
        focus_grabber_factory_.NewRunnableMethod(
            &NewUserView::FocusFirstField));
    WizardAccessibilityHelper::GetInstance()->MaybeEnableAccessibility(this);
  } else if (is_add && (child == username_field_ || child == password_field_)) {
    MessageLoop::current()->PostTask(FROM_HERE,
        focus_grabber_factory_.NewRunnableMethod(
            &NewUserView::Layout));
  }
}

void NewUserView::NativeViewHierarchyChanged(bool attached,
                                             gfx::NativeView native_view,
                                             views::RootView* root_view) {
  if (focus_delayed_ && attached) {
    focus_delayed_ = false;
    MessageLoop::current()->PostTask(FROM_HERE,
        focus_grabber_factory_.NewRunnableMethod(
            &NewUserView::FocusFirstField));
  }
}

void NewUserView::FocusFirstField() {
  if (GetFocusManager()) {
    if (username_field_->text().empty())
      username_field_->RequestFocus();
    else
      password_field_->RequestFocus();
  } else {
    // We are invisible - delay until it is no longer the case.
    focus_delayed_ = true;
  }
}

// Sets the bounds of the view, using x and y as the origin.
// The width is determined by the min of width and the preferred size
// of the view, unless force_width is true in which case it is always used.
// The height is gotten from the preferred size and returned.
static int setViewBounds(
    views::View* view, int x, int y, int width, bool force_width) {
  gfx::Size pref_size = view->GetPreferredSize();
  if (!force_width) {
    if (pref_size.width() < width) {
      width = pref_size.width();
    }
  }
  int height = pref_size.height();
  view->SetBounds(x, y, width, height);
  return height;
}

void NewUserView::Layout() {
  gfx::Insets insets = GetInsets();

  // Place language selection in top right corner.
  int x = std::max(0,
      this->width() - insets.right() -
          languages_menubutton_->GetPreferredSize().width() - kColumnPad);
  int y = insets.top() + kLanguagesMenuPad;
  int width = std::min(this->width() - insets.width() - 2 * kColumnPad,
                       languages_menubutton_->GetPreferredSize().width());
  int height = kLanguagesMenuHeight;
  languages_menubutton_->SetBounds(x, y, width, height);
  y += height + kTitlePad;

  width = std::min(this->width() - insets.width() - 2 * kColumnPad,
                   kTextfieldWidth);
  x = insets.left() + kLeftPad;
  int max_width = this->width() - x - std::max(insets.right(), x);
  title_label_->SizeToFit(max_width);
  title_hint_label_->SizeToFit(max_width);

  // Top align title and title hint.
  y += setViewBounds(title_label_, x, y, max_width, false);
  y += setViewBounds(title_hint_label_, x, y, max_width, false);
  int title_end = y + kTitlePad;

  splitter_up1_->SetBounds(0, title_end, this->width(), kSplitterHeight);
  splitter_up2_->SetBounds(0, title_end + 1, this->width(), kSplitterHeight);

  // Bottom controls.
  int links_height = 0;
  if (need_create_account_)
    links_height += create_account_link_->GetPreferredSize().height();
  if (need_guest_link_)
    links_height += guest_link_->GetPreferredSize().height();
  if (need_create_account_ && need_guest_link_)
    links_height += kRowPad;
  y = this->height() - insets.bottom() - kBottomPad;
  if (links_height)
    y -= links_height + kBottomPad;
  int bottom_start = y;

  splitter_down1_->SetBounds(0, y, this->width(), kSplitterHeight);
  splitter_down2_->SetBounds(0, y + 1, this->width(), kSplitterHeight);

  if (need_guest_link_) {
    y -= setViewBounds(guest_link_,
                       x, y + kBottomPad, max_width, false) + kRowPad;
  }
  if (need_create_account_) {
    y += setViewBounds(create_account_link_, x, y, max_width, false);
  }

  // Center main controls.
  height = username_field_->GetPreferredSize().height() +
           password_field_->GetPreferredSize().height() +
           sign_in_button_->GetPreferredSize().height() +
           2 * kRowPad;
  y = title_end + (bottom_start - title_end - height) / 2;

  y += setViewBounds(username_field_, x, y, width, true) + kRowPad;
  y += setViewBounds(password_field_, x, y, width, true) + kRowPad;

  int sign_in_button_width = sign_in_button_->GetPreferredSize().width();
  setViewBounds(sign_in_button_, x, y, sign_in_button_width,true);

  SchedulePaint();
}

gfx::Size NewUserView::GetPreferredSize() {
  return need_guest_link_ ?
      gfx::Size(kNewUserPodFullWidth, kNewUserPodFullHeight) :
      gfx::Size(kNewUserPodSmallWidth, kNewUserPodSmallHeight);
}

void NewUserView::SetUsername(const std::string& username) {
  username_field_->SetText(UTF8ToUTF16(username));
}

void NewUserView::SetPassword(const std::string& password) {
  password_field_->SetText(UTF8ToUTF16(password));
}

void NewUserView::Login() {
  if (login_in_process_ || username_field_->text().empty())
    return;

  login_in_process_ = true;
  std::string username = UTF16ToUTF8(username_field_->text());
  // todo(cmasone) Need to sanitize memory used to store password.
  std::string password = UTF16ToUTF8(password_field_->text());

  if (username.find('@') == std::string::npos) {
    username += kDefaultDomain;
    username_field_->SetText(UTF8ToUTF16(username));
  }

  delegate_->OnLogin(username, password);
}

// Sign in button causes a login attempt.
void NewUserView::ButtonPressed(views::Button* sender,
                                const views::Event& event) {
  DCHECK(sender == sign_in_button_);
  Login();
}

void NewUserView::LinkActivated(views::Link* source, int event_flags) {
  if (source == create_account_link_) {
    delegate_->OnCreateAccount();
  } else if (source == guest_link_) {
    delegate_->OnLoginAsGuest();
  }
}

void NewUserView::ClearAndFocusControls() {
  login_in_process_ = false;
  SetUsername(std::string());
  SetPassword(std::string());
  username_field_->RequestFocus();
}

void NewUserView::ClearAndFocusPassword() {
  login_in_process_ = false;
  SetPassword(std::string());
  password_field_->RequestFocus();
}

gfx::Rect NewUserView::GetMainInputScreenBounds() const {
  return GetUsernameBounds();
}

gfx::Rect NewUserView::CalculateThrobberBounds(views::Throbber* throbber) {
  DCHECK(password_field_);
  DCHECK(sign_in_button_);

  gfx::Size throbber_size = throbber->GetPreferredSize();
  int x = password_field_->x();
  x += password_field_->width() - throbber_size.width();
  int y = sign_in_button_->y();
  y += (sign_in_button_->height() - throbber_size.height()) / 2;

  return gfx::Rect(gfx::Point(x, y), throbber_size);
}

gfx::Rect NewUserView::GetPasswordBounds() const {
  return password_field_->GetScreenBounds();
}

gfx::Rect NewUserView::GetUsernameBounds() const {
  return username_field_->GetScreenBounds();
}

bool NewUserView::HandleKeyEvent(views::Textfield* sender,
                                 const views::KeyEvent& key_event) {
  if (!CrosLibrary::Get()->EnsureLoaded() || login_in_process_)
    return false;

  if (key_event.GetKeyCode() == ui::VKEY_RETURN) {
    if (!username_field_->text().empty() && !password_field_->text().empty())
      Login();
    // Return true so that processing ends
    return true;
  }
  delegate_->ClearErrors();
  // Return false so that processing does not end
  return false;
}

void NewUserView::ContentsChanged(views::Textfield* sender,
                                  const string16& new_contents) {
  UpdateSignInButtonState();
  if (!new_contents.empty())
    delegate_->ClearErrors();
}

void NewUserView::EnableInputControls(bool enabled) {
  languages_menubutton_->SetEnabled(enabled);
  username_field_->SetEnabled(enabled);
  password_field_->SetEnabled(enabled);
  sign_in_button_->SetEnabled(enabled);
  if (need_create_account_) {
    create_account_link_->SetEnabled(enabled);
  }
  if (need_guest_link_) {
    guest_link_->SetEnabled(enabled);
  }
}

bool NewUserView::NavigateAway() {
  if (username_field_->text().empty() &&
      password_field_->text().empty()) {
    delegate_->NavigateAway();
    return true;
  } else {
    return false;
  }
}

void NewUserView::InitLink(views::Link** link) {
  *link = new views::Link(std::wstring());
  (*link)->SetController(this);
  (*link)->SetNormalColor(login::kLinkColor);
  (*link)->SetHighlightedColor(login::kLinkColor);
  AddChildView(*link);
}

}  // namespace chromeos

