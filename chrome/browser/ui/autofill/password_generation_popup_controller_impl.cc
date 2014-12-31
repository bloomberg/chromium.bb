// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/password_generation_popup_controller_impl.h"

#include <math.h>

#include "base/i18n/rtl.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/password_generation_popup_observer.h"
#include "chrome/browser/ui/autofill/password_generation_popup_view.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/password_generator.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/text_utils.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chromium_application.h"
#endif

namespace autofill {

base::WeakPtr<PasswordGenerationPopupControllerImpl>
PasswordGenerationPopupControllerImpl::GetOrCreate(
    base::WeakPtr<PasswordGenerationPopupControllerImpl> previous,
    const gfx::RectF& bounds,
    const PasswordForm& form,
    int max_length,
    password_manager::PasswordManager* password_manager,
    password_manager::PasswordManagerDriver* driver,
    PasswordGenerationPopupObserver* observer,
    content::WebContents* web_contents,
    gfx::NativeView container_view) {
  if (previous.get() &&
      previous->element_bounds() == bounds &&
      previous->web_contents() == web_contents &&
      previous->container_view() == container_view) {
    return previous;
  }

  if (previous.get())
    previous->Hide();

  PasswordGenerationPopupControllerImpl* controller =
      new PasswordGenerationPopupControllerImpl(
          bounds, form, max_length, password_manager, driver, observer,
          web_contents, container_view);
  return controller->GetWeakPtr();
}

PasswordGenerationPopupControllerImpl::PasswordGenerationPopupControllerImpl(
    const gfx::RectF& bounds,
    const PasswordForm& form,
    int max_length,
    password_manager::PasswordManager* password_manager,
    password_manager::PasswordManagerDriver* driver,
    PasswordGenerationPopupObserver* observer,
    content::WebContents* web_contents,
    gfx::NativeView container_view)
    : view_(NULL),
      form_(form),
      password_manager_(password_manager),
      driver_(driver),
      observer_(observer),
      generator_(new PasswordGenerator(max_length)),
      controller_common_(bounds, container_view, web_contents),
      password_selected_(false),
      display_password_(false),
      weak_ptr_factory_(this) {
  controller_common_.SetKeyPressCallback(
      base::Bind(&PasswordGenerationPopupControllerImpl::HandleKeyPressEvent,
                 base::Unretained(this)));

  std::vector<base::string16> pieces;
  base::SplitStringDontTrim(
      l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_PROMPT),
      '|',  // separator
      &pieces);
  DCHECK_EQ(3u, pieces.size());
  link_range_ = gfx::Range(pieces[0].size(),
                           pieces[0].size() + pieces[1].size());
  help_text_ = JoinString(pieces, base::string16());
}

PasswordGenerationPopupControllerImpl::~PasswordGenerationPopupControllerImpl()
  {}

base::WeakPtr<PasswordGenerationPopupControllerImpl>
PasswordGenerationPopupControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool PasswordGenerationPopupControllerImpl::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  switch (event.windowsKeyCode) {
    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
      PasswordSelected(true);
      return true;
    case ui::VKEY_ESCAPE:
      Hide();
      return true;
    case ui::VKEY_RETURN:
    case ui::VKEY_TAB:
      // We suppress tab if the password is selected because we will
      // automatically advance focus anyway.
      return PossiblyAcceptPassword();
    default:
      return false;
  }
}

bool PasswordGenerationPopupControllerImpl::PossiblyAcceptPassword() {
  if (password_selected_) {
    PasswordAccepted();  // This will delete |this|.
    return true;
  }

  return false;
}

void PasswordGenerationPopupControllerImpl::PasswordSelected(bool selected) {
  if (!display_password_ || selected == password_selected_)
    return;

  password_selected_ = selected;
  view_->PasswordSelectionUpdated();
  view_->UpdateBoundsAndRedrawPopup();
}

void PasswordGenerationPopupControllerImpl::PasswordAccepted() {
  if (!display_password_)
    return;

  driver_->GeneratedPasswordAccepted(current_password_);
  password_manager_->SetFormHasGeneratedPassword(driver_, form_);
  Hide();
}

int PasswordGenerationPopupControllerImpl::GetMinimumWidth() {
  // Minimum width in pixels.
  const int minimum_width = 350;

  // If the width of the field is longer than the minimum, use that instead.
  return std::max(minimum_width,
                  controller_common_.RoundedElementBounds().width());
}

void PasswordGenerationPopupControllerImpl::CalculateBounds() {
  gfx::Size bounds = view_->GetPreferredSizeOfPasswordView();

  popup_bounds_ = controller_common_.GetPopupBounds(bounds.width(),
                                                    bounds.height());
}

void PasswordGenerationPopupControllerImpl::Show(bool display_password) {
  display_password_ = display_password;
  if (display_password_ && current_password_.empty())
    current_password_ = base::ASCIIToUTF16(generator_->Generate());

  if (!view_) {
    view_ = PasswordGenerationPopupView::Create(this);

    // Treat popup as being hidden if creation fails.
    if (!view_) {
      Hide();
      return;
    }

    CalculateBounds();
    view_->Show();
  } else {
    CalculateBounds();
    view_->UpdateBoundsAndRedrawPopup();
  }

  controller_common_.RegisterKeyPressCallback();

  if (observer_)
    observer_->OnPopupShown(display_password_);
}

void PasswordGenerationPopupControllerImpl::HideAndDestroy() {
  Hide();
}

void PasswordGenerationPopupControllerImpl::Hide() {
  controller_common_.RemoveKeyPressCallback();

  if (view_)
    view_->Hide();

  if (observer_)
    observer_->OnPopupHidden();

  delete this;
}

void PasswordGenerationPopupControllerImpl::ViewDestroyed() {
  view_ = NULL;

  Hide();
}

void PasswordGenerationPopupControllerImpl::OnSavedPasswordsLinkClicked() {
#if defined(OS_ANDROID)
  chrome::android::ChromiumApplication::ShowPasswordSettings();
#else
  chrome::ShowSettingsSubPage(
      chrome::FindBrowserWithWebContents(controller_common_.web_contents()),
      chrome::kPasswordManagerSubPage);
#endif
}

void PasswordGenerationPopupControllerImpl::SetSelectionAtPoint(
    const gfx::Point& point) {
  PasswordSelected(view_->IsPointInPasswordBounds(point));
}

bool PasswordGenerationPopupControllerImpl::AcceptSelectedLine() {
  if (!password_selected_)
    return false;

  PasswordAccepted();
  return true;
}

void PasswordGenerationPopupControllerImpl::SelectionCleared() {
  PasswordSelected(false);
}

gfx::NativeView PasswordGenerationPopupControllerImpl::container_view() {
  return controller_common_.container_view();
}

const gfx::Rect& PasswordGenerationPopupControllerImpl::popup_bounds() const {
  return popup_bounds_;
}

const gfx::RectF& PasswordGenerationPopupControllerImpl::element_bounds()
    const {
  return controller_common_.element_bounds();
}

bool PasswordGenerationPopupControllerImpl::IsRTL() const {
  return base::i18n::IsRTL();
}

bool PasswordGenerationPopupControllerImpl::display_password() const {
  return display_password_;
}

bool PasswordGenerationPopupControllerImpl::password_selected() const {
  return password_selected_;
}

base::string16 PasswordGenerationPopupControllerImpl::password() const {
  return current_password_;
}

base::string16 PasswordGenerationPopupControllerImpl::SuggestedText() {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_SUGGESTION);
}

const base::string16& PasswordGenerationPopupControllerImpl::HelpText() {
  return help_text_;
}

const gfx::Range& PasswordGenerationPopupControllerImpl::HelpTextLinkRange() {
  return link_range_;
}

}  // namespace autofill
