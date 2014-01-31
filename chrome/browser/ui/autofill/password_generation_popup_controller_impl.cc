// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/password_generation_popup_controller_impl.h"

#include <math.h>

#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/ui/autofill/password_generation_popup_observer.h"
#include "chrome/browser/ui/autofill/password_generation_popup_view.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/password_generator.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/rect_conversions.h"

namespace autofill {

const int kMinimumWidth = 60;
const int kDividerHeight = 1;

base::WeakPtr<PasswordGenerationPopupControllerImpl>
PasswordGenerationPopupControllerImpl::GetOrCreate(
    base::WeakPtr<PasswordGenerationPopupControllerImpl> previous,
    const gfx::RectF& bounds,
    const PasswordForm& form,
    PasswordGenerator* generator,
    PasswordManager* password_manager,
    PasswordGenerationPopupObserver* observer,
    content::WebContents* web_contents,
    gfx::NativeView container_view) {
  if (previous.get() &&
      previous->element_bounds() == bounds &&
      previous->web_contents() == web_contents &&
      previous->container_view() == container_view) {
    // TODO(gcasto): Should we clear state here?
    return previous;
  }

  if (previous.get())
    previous->Hide();

  PasswordGenerationPopupControllerImpl* controller =
      new PasswordGenerationPopupControllerImpl(
          bounds,
          form,
          generator,
          password_manager,
          observer,
          web_contents,
          container_view);
  return controller->GetWeakPtr();
}

PasswordGenerationPopupControllerImpl::PasswordGenerationPopupControllerImpl(
    const gfx::RectF& bounds,
    const PasswordForm& form,
    PasswordGenerator* generator,
    PasswordManager* password_manager,
    PasswordGenerationPopupObserver* observer,
    content::WebContents* web_contents,
    gfx::NativeView container_view)
    : form_(form),
      generator_(generator),
      password_manager_(password_manager),
      observer_(observer),
      controller_common_(bounds, container_view, web_contents),
      view_(NULL),
      current_password_(base::ASCIIToUTF16(generator->Generate())),
      password_selected_(false),
      weak_ptr_factory_(this) {
  controller_common_.SetKeyPressCallback(
      base::Bind(&PasswordGenerationPopupControllerImpl::HandleKeyPressEvent,
                 base::Unretained(this)));
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
      // We supress tab if the password is selected because we will
      // automatically advance focus anyway.
      return PossiblyAcceptPassword();
    default:
      return false;
  }
}

bool PasswordGenerationPopupControllerImpl::PossiblyAcceptPassword() {
  if (password_selected_)
    PasswordAccepted();

  return password_selected_;
}

void PasswordGenerationPopupControllerImpl::PasswordSelected(bool selected) {
  password_selected_ = selected;
  view_->UpdateBoundsAndRedrawPopup();
}

void PasswordGenerationPopupControllerImpl::PasswordAccepted() {
  web_contents()->GetRenderViewHost()->Send(
      new AutofillMsg_GeneratedPasswordAccepted(
          web_contents()->GetRenderViewHost()->GetRoutingID(),
          current_password_));
  password_manager_->SetFormHasGeneratedPassword(form_);
  Hide();
}

int PasswordGenerationPopupControllerImpl::GetDesiredWidth() {
  // Minimum width we want to display the password.
  int minimum_length_for_text = 2 * kHorizontalPadding +
      font_.GetExpectedTextWidth(kMinimumWidth) +
      2 * kPopupBorderThickness;

  // If the width of the field is longer than the minimum, use that instead.
  return std::max(minimum_length_for_text,
                  controller_common_.RoundedElementBounds().width());
}

int PasswordGenerationPopupControllerImpl::GetDesiredHeight(int width) {
  // Note that this wrapping isn't exactly what the popup will do. It shouldn't
  // line break in the middle of the link, but as long as the link isn't longer
  // than given width this shouldn't affect the height calculated here. The
  // default width should be wide enough to prevent this from being an issue.
  int total_length = font_.GetStringWidth(HelpText() + LearnMoreLink());
  int usable_width = width - 2 * kHorizontalPadding;
  int text_height =
      static_cast<int>(ceil(static_cast<double>(total_length)/usable_width)) *
      font_.GetHeight();
  int help_section_height = text_height + 2 * kHelpVerticalPadding;

  int password_section_height =
      font_.GetHeight() + 2 * kPasswordVerticalPadding;

  return (2 * kPopupBorderThickness +
          help_section_height +
          password_section_height);
}

void PasswordGenerationPopupControllerImpl::CalculateBounds() {
  int popup_width  = GetDesiredWidth();
  int popup_height = GetDesiredHeight(popup_width);

  popup_bounds_ = controller_common_.GetPopupBounds(popup_height, popup_width);

  // Calculate the bounds for the rest of the elements given the bounds of
  // the popup.
  password_bounds_ =  gfx::Rect(
      kPopupBorderThickness,
      kPopupBorderThickness,
      popup_bounds_.width() - 2 * kPopupBorderThickness,
      font_.GetHeight() + 2 * kPasswordVerticalPadding);

  divider_bounds_ = gfx::Rect(kPopupBorderThickness,
                              password_bounds_.bottom(),
                              password_bounds_.width(),
                              kDividerHeight);

  int help_height =
      popup_bounds_.height() - divider_bounds_.bottom() - kPopupBorderThickness;
  help_bounds_ = gfx::Rect(
      kPopupBorderThickness,
      divider_bounds_.bottom(),
      password_bounds_.width(),
      help_height);
}

void PasswordGenerationPopupControllerImpl::Show() {
  CalculateBounds();

  if (!view_) {
    view_ = PasswordGenerationPopupView::Create(this);
    view_->Show();
  }

  controller_common_.RegisterKeyPressCallback();

  if (observer_)
    observer_->OnPopupShown();
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

void PasswordGenerationPopupControllerImpl::OnHelpLinkClicked() {
  Browser* browser =
      chrome::FindBrowserWithWebContents(controller_common_.web_contents());
  content::OpenURLParams params(
      GURL(chrome::kAutoPasswordGenerationLearnMoreURL), content::Referrer(),
      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK, false);
  browser->OpenURL(params);
}

void PasswordGenerationPopupControllerImpl::SetSelectionAtPoint(
    const gfx::Point& point) {
  if (password_bounds_.Contains(point))
    PasswordSelected(true);
}

void PasswordGenerationPopupControllerImpl::AcceptSelectionAtPoint(
    const gfx::Point& point) {
  if (password_bounds_.Contains(point))
    PasswordAccepted();
}

void PasswordGenerationPopupControllerImpl::SelectionCleared() {
  PasswordSelected(false);
}

bool PasswordGenerationPopupControllerImpl::ShouldRepostEvent(
    const ui::MouseEvent& event) {
  return false;
}

bool PasswordGenerationPopupControllerImpl::ShouldHideOnOutsideClick() const {
  // Will be hidden when focus changes anyway.
  return false;
}

gfx::NativeView PasswordGenerationPopupControllerImpl::container_view() {
  return controller_common_.container_view();
}

const gfx::Rect& PasswordGenerationPopupControllerImpl::popup_bounds() const {
  return popup_bounds_;
}

const gfx::Rect& PasswordGenerationPopupControllerImpl::password_bounds()
    const {
  return password_bounds_;
}

const gfx::Rect& PasswordGenerationPopupControllerImpl::divider_bounds()
    const {
  return divider_bounds_;
}

const gfx::Rect& PasswordGenerationPopupControllerImpl::help_bounds() const {
  return help_bounds_;
}

bool PasswordGenerationPopupControllerImpl::password_selected() const {
  return password_selected_;
}

base::string16 PasswordGenerationPopupControllerImpl::password() const {
  return current_password_;
}

base::string16 PasswordGenerationPopupControllerImpl::HelpText() {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_PROMPT);
}

base::string16 PasswordGenerationPopupControllerImpl::LearnMoreLink() {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_LEARN_MORE_LINK);
}

}  // namespace autofill
