// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/password_generation_popup_controller_impl.h"

#include <math.h>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/password_generation_popup_observer.h"
#include "chrome/browser/ui/autofill/password_generation_popup_view.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/password_generator.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/text_utils.h"

namespace autofill {

base::WeakPtr<PasswordGenerationPopupControllerImpl>
PasswordGenerationPopupControllerImpl::GetOrCreate(
    base::WeakPtr<PasswordGenerationPopupControllerImpl> previous,
    const gfx::RectF& bounds,
    const PasswordForm& form,
    int max_length,
    password_manager::PasswordManager* password_manager,
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
          bounds,
          form,
          max_length,
          password_manager,
          observer,
          web_contents,
          container_view);
  return controller->GetWeakPtr();
}

PasswordGenerationPopupControllerImpl::PasswordGenerationPopupControllerImpl(
    const gfx::RectF& bounds,
    const PasswordForm& form,
    int max_length,
    password_manager::PasswordManager* password_manager,
    PasswordGenerationPopupObserver* observer,
    content::WebContents* web_contents,
    gfx::NativeView container_view)
    : form_(form),
      password_manager_(password_manager),
      observer_(observer),
      generator_(new PasswordGenerator(max_length)),
      controller_common_(bounds, container_view, web_contents),
      view_(NULL),
      font_list_(ResourceBundle::GetSharedInstance().GetFontList(
          ResourceBundle::SmallFont)),
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
  if (!display_password_)
    return;

  password_selected_ = selected;
  view_->PasswordSelectionUpdated();
  view_->UpdateBoundsAndRedrawPopup();
}

void PasswordGenerationPopupControllerImpl::PasswordAccepted() {
  if (!display_password_)
    return;

  web_contents()->GetRenderViewHost()->Send(
      new AutofillMsg_GeneratedPasswordAccepted(
          web_contents()->GetRenderViewHost()->GetRoutingID(),
          current_password_));
  password_manager_->SetFormHasGeneratedPassword(form_);
  Hide();
}

int PasswordGenerationPopupControllerImpl::GetDesiredWidth() {
  // Minimum width in pixels.
  const int minimum_required_width = 300;

  // If the width of the field is longer than the minimum, use that instead.
  int width = std::max(minimum_required_width,
                       controller_common_.RoundedElementBounds().width());

  if (display_password_) {
    // Make sure that the width will always be large enough to display the
    // password and suggestion on one line.
    width = std::max(width,
                     gfx::GetStringWidth(current_password_ + SuggestedText(),
                                         font_list_) + 2 * kHorizontalPadding);
  }

  return width;
}

int PasswordGenerationPopupControllerImpl::GetDesiredHeight(int width) {
  // Note that this wrapping isn't exactly what the popup will do. It shouldn't
  // line break in the middle of the link, but as long as the link isn't longer
  // than given width this shouldn't affect the height calculated here. The
  // default width should be wide enough to prevent this from being an issue.
  int total_length = gfx::GetStringWidth(HelpText(), font_list_);
  int usable_width = width - 2 * kHorizontalPadding;
  int text_height =
      static_cast<int>(ceil(static_cast<double>(total_length)/usable_width)) *
      font_list_.GetFontSize();
  int help_section_height = text_height + 2 * kHelpVerticalPadding;

  int password_section_height = 0;
  if (display_password_) {
    password_section_height =
        font_list_.GetFontSize() + 2 * kPasswordVerticalPadding;
  }

  return (2 * kPopupBorderThickness +
          help_section_height +
          password_section_height);
}

void PasswordGenerationPopupControllerImpl::CalculateBounds() {
  int popup_width  = GetDesiredWidth();
  int popup_height = GetDesiredHeight(popup_width);

  popup_bounds_ = controller_common_.GetPopupBounds(popup_height, popup_width);
  int sub_view_width = popup_bounds_.width() - 2 * kPopupBorderThickness;

  // Calculate the bounds for the rest of the elements given the bounds of
  // the popup.
  if (display_password_) {
    password_bounds_ =  gfx::Rect(
        kPopupBorderThickness,
        kPopupBorderThickness,
        sub_view_width,
        font_list_.GetFontSize() + 2 * kPasswordVerticalPadding);

    divider_bounds_ = gfx::Rect(kPopupBorderThickness,
                                password_bounds_.bottom(),
                                sub_view_width,
                                1 /* divider heigth*/);
  } else {
    password_bounds_ = gfx::Rect();
    divider_bounds_ = gfx::Rect();
  }

  int help_y = std::max(kPopupBorderThickness, divider_bounds_.bottom());
  int help_height =
      popup_bounds_.height() - help_y - kPopupBorderThickness;
  help_bounds_ = gfx::Rect(
      kPopupBorderThickness,
      help_y,
      sub_view_width,
      help_height);
}

void PasswordGenerationPopupControllerImpl::Show(bool display_password) {
  display_password_ = display_password;
  if (display_password_)
    current_password_ = base::ASCIIToUTF16(generator_->Generate());

  CalculateBounds();

  if (!view_) {
    view_ = PasswordGenerationPopupView::Create(this);
    view_->Show();
  } else {
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
  // TODO(gcasto): Change this to navigate to account central once passwords
  // are visible there.
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

const gfx::FontList& PasswordGenerationPopupControllerImpl::font_list() const {
  return font_list_;
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
