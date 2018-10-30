// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

using content::WebContents;

LocationIconView::LocationIconView(const gfx::FontList& font_list,
                                   Delegate* delegate)
    : IconLabelBubbleView(font_list), delegate_(delegate) {
  label()->SetElideBehavior(gfx::ELIDE_MIDDLE);
  set_id(VIEW_ID_LOCATION_ICON);
  Update();
  SetUpForInOutAnimation();
}

LocationIconView::~LocationIconView() {
}

gfx::Size LocationIconView::GetMinimumSize() const {
  return GetMinimumSizeForPreferredSize(GetPreferredSize());
}

bool LocationIconView::OnMousePressed(const ui::MouseEvent& event) {
  delegate_->OnLocationIconPressed(event);

  IconLabelBubbleView::OnMousePressed(event);
  return true;
}

bool LocationIconView::OnMouseDragged(const ui::MouseEvent& event) {
  delegate_->OnLocationIconDragged(event);
  return IconLabelBubbleView::OnMouseDragged(event);
}

bool LocationIconView::GetTooltipText(const gfx::Point& p,
                                      base::string16* tooltip) const {
  if (show_tooltip_)
    *tooltip = l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON);
  return show_tooltip_;
}

SkColor LocationIconView::GetTextColor() const {
  return delegate_->GetSecurityChipColor(
      delegate_->GetLocationBarModel()->GetSecurityLevel(false));
}

bool LocationIconView::ShouldShowSeparator() const {
  if (ShouldShowLabel())
    return true;

  if (OmniboxFieldTrial::IsJogTextfieldOnPopupEnabled())
    return false;

  return !delegate_->IsEditingOrEmpty();
}

bool LocationIconView::ShouldShowExtraEndSpace() const {
  if (OmniboxFieldTrial::IsJogTextfieldOnPopupEnabled())
    return false;

  return delegate_->IsEditingOrEmpty();
}

bool LocationIconView::ShowBubble(const ui::Event& event) {
  return delegate_->ShowPageInfoDialog();
}

SkColor LocationIconView::GetInkDropBaseColor() const {
  return delegate_->GetLocationIconInkDropColor();
}

void LocationIconView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (delegate_->IsEditingOrEmpty()) {
    node_data->role = ax::mojom::Role::kImage;
    node_data->SetName(l10n_util::GetStringUTF8(IDS_ACC_SEARCH_ICON));
    return;
  }

  security_state::SecurityLevel security_level =
      delegate_->GetLocationBarModel()->GetSecurityLevel(false);
  if (label()->text().empty() && (security_level == security_state::EV_SECURE ||
                                  security_level == security_state::SECURE)) {
    node_data->AddStringAttribute(
        ax::mojom::StringAttribute::kDescription,
        l10n_util::GetStringUTF8(IDS_SECURE_VERBOSE_STATE));
  }

  IconLabelBubbleView::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kPopUpButton;
}

bool LocationIconView::IsBubbleShowing() const {
  return PageInfoBubbleView::GetShownBubbleType() !=
         PageInfoBubbleView::BUBBLE_NONE;
}

int LocationIconView::GetMinimumLabelTextWidth() const {
  int width = 0;

  base::string16 text = GetText();
  if (text == label()->text()) {
    // Optimize this common case by not creating a new label.
    // GetPreferredSize is not dependent on the label's current
    // width, so this returns the same value as the branch below.
    width = label()->GetPreferredSize().width();
  } else {
    views::Label label(text, {font_list()});
    width = label.GetPreferredSize().width();
  }
  return GetMinimumSizeForPreferredSize(GetSizeForLabelWidth(width)).width();
}

bool LocationIconView::ShouldShowText() const {
  const auto* location_bar_model = delegate_->GetLocationBarModel();

  if (!location_bar_model->input_in_progress()) {
    const GURL& url = location_bar_model->GetURL();
    if (url.SchemeIs(content::kChromeUIScheme) ||
        url.SchemeIs(extensions::kExtensionScheme) ||
        url.SchemeIs(url::kFileScheme))
      return true;
  }

  return !location_bar_model->GetSecureVerboseText().empty();
}

base::string16 LocationIconView::GetText() const {
  if (delegate_->GetLocationBarModel()->GetURL().SchemeIs(
          content::kChromeUIScheme))
    return l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);

  if (delegate_->GetLocationBarModel()->GetURL().SchemeIs(url::kFileScheme))
    return l10n_util::GetStringUTF16(IDS_OMNIBOX_FILE);

  if (delegate_->GetWebContents()) {
    // On ChromeOS, this can be called using web_contents from
    // SimpleWebViewDialog::GetWebContents() which always returns null.
    // TODO(crbug.com/680329) Remove the null check and make
    // SimpleWebViewDialog::GetWebContents return the proper web contents
    // instead.
    const base::string16 extension_name =
        extensions::ui_util::GetEnabledExtensionNameForUrl(
            delegate_->GetLocationBarModel()->GetURL(),
            delegate_->GetWebContents()->GetBrowserContext());
    if (!extension_name.empty())
      return extension_name;
  }

  return delegate_->GetLocationBarModel()->GetSecureVerboseText();
}

bool LocationIconView::ShouldAnimateTextVisibilityChange() const {
  using SecurityLevel = security_state::SecurityLevel;
  SecurityLevel level =
      delegate_->GetLocationBarModel()->GetSecurityLevel(false);
  // Do not animate transitions from HTTP_SHOW_WARNING to DANGEROUS, since the
  // transition can look confusing/messy.
  if (level == SecurityLevel::DANGEROUS &&
      last_update_security_level_ == SecurityLevel::HTTP_SHOW_WARNING)
    return false;
  return (level == SecurityLevel::DANGEROUS ||
          level == SecurityLevel::HTTP_SHOW_WARNING);
}

void LocationIconView::UpdateTextVisibility() {
  SetLabel(GetText());

  bool should_show = ShouldShowText();
  if (!ShouldAnimateTextVisibilityChange())
    ResetSlideAnimation(should_show);
  else if (should_show)
    AnimateIn(base::nullopt);
  else
    AnimateOut();

  // The label text color may have changed.
  OnNativeThemeChanged(GetNativeTheme());
}

void LocationIconView::UpdateIcon() {
  // Cancel any previous outstanding icon requests, as they are now outdated.
  icon_fetch_weak_ptr_factory_.InvalidateWeakPtrs();

  gfx::ImageSkia icon = delegate_->GetLocationIcon(
      base::BindOnce(&LocationIconView::OnIconFetched,
                     icon_fetch_weak_ptr_factory_.GetWeakPtr()));

  SetImage(icon);
}

void LocationIconView::OnIconFetched(const gfx::Image& image) {
  SetImage(image.AsImageSkia());
}

void LocationIconView::Update() {
  UpdateTextVisibility();
  UpdateIcon();

  bool is_editing_or_empty = delegate_->IsEditingOrEmpty();
  // The tooltip should be shown if we are not editing or empty.
  show_tooltip_ = !is_editing_or_empty;

  // If the omnibox is empty or editing, the user should not be able to left
  // click on the icon. As such, the icon should not show a highlight or be
  // focusable. Note: using the middle mouse to copy-and-paste should still
  // work on the icon.
  if (is_editing_or_empty) {
    SetInkDropMode(InkDropMode::OFF);
    SetFocusBehavior(FocusBehavior::NEVER);
    return;
  }

  SetInkDropMode(InkDropMode::ON);

#if defined(OS_MACOSX)
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif

  last_update_security_level_ =
      delegate_->GetLocationBarModel()->GetSecurityLevel(false);
}

bool LocationIconView::IsTriggerableEvent(const ui::Event& event) {
  if (delegate_->IsEditingOrEmpty())
    return false;

  if (event.IsMouseEvent()) {
    if (event.AsMouseEvent()->IsOnlyMiddleMouseButton())
      return false;
  } else if (event.IsGestureEvent() && event.type() != ui::ET_GESTURE_TAP) {
    return false;
  }

  return IconLabelBubbleView::IsTriggerableEvent(event);
}

double LocationIconView::WidthMultiplier() const {
  return GetAnimationValue();
}

gfx::Size LocationIconView::GetMinimumSizeForPreferredSize(
    gfx::Size size) const {
  const int kMinCharacters = 10;
  size.SetToMin(
      GetSizeForLabelWidth(font_list().GetExpectedTextWidth(kMinCharacters)));
  return size;
}

int LocationIconView::GetSlideDurationTime() const {
  constexpr int kSlideDurationTimeMs = 150;
  return kSlideDurationTimeMs;
}
