// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h"

#include <stddef.h>

#include "base/strings/string16.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row_observer.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace {

// (Square) pixel size of icon.
constexpr int kIconSize = 18;

// The type of arrow to display on the permission bubble.
constexpr views::BubbleBorder::Arrow kPermissionAnchorArrow =
    views::BubbleBorder::TOP_LEFT;

// Returns the view to anchor the permission bubble to. May be null.
views::View* GetPermissionAnchorView(Browser* browser) {
  return bubble_anchor_util::GetPageInfoAnchorView(browser);
}

// Returns the anchor rect to anchor the permission bubble to, as a fallback.
// Only used if GetPermissionAnchorView() returns nullptr.
gfx::Rect GetPermissionAnchorRect(Browser* browser) {
  return bubble_anchor_util::GetPageInfoAnchorRect(browser);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// View implementation for the permissions bubble.
class PermissionsBubbleDialogDelegateView
    : public views::BubbleDialogDelegateView {
 public:
  PermissionsBubbleDialogDelegateView(
      PermissionPromptImpl* owner,
      const std::vector<PermissionRequest*>& requests);
  ~PermissionsBubbleDialogDelegateView() override;

  void CloseBubble();

  // BubbleDialogDelegateView:
  ui::AXRole GetAccessibleWindowRole() const override;
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;
  void AddedToWidget() override;
  void OnWidgetDestroying(views::Widget* widget) override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  int GetDefaultDialogButton() const override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  void SizeToContents() override;

  // Repositions the bubble so it's displayed in the correct location based on
  // the updated anchor view, or anchor rect if that is (or became) null.
  void UpdateAnchor();

 private:
  PermissionPromptImpl* owner_;
  base::string16 display_origin_;
  views::Checkbox* persist_checkbox_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsBubbleDialogDelegateView);
};

PermissionsBubbleDialogDelegateView::PermissionsBubbleDialogDelegateView(
    PermissionPromptImpl* owner,
    const std::vector<PermissionRequest*>& requests)
    : owner_(owner), persist_checkbox_(nullptr) {
  DCHECK(!requests.empty());

  set_close_on_deactivate(false);
  set_arrow(kPermissionAnchorArrow);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  display_origin_ = url_formatter::FormatUrlForSecurityDisplay(
      requests[0]->GetOrigin(),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

  bool show_persistence_toggle = true;
  for (size_t index = 0; index < requests.size(); index++) {
    views::View* label_container = new views::View();
    int indent =
        provider->GetDistanceMetric(DISTANCE_SUBSECTION_HORIZONTAL_INDENT);
    label_container->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, gfx::Insets(0, indent),
        provider->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL)));
    views::ImageView* icon = new views::ImageView();
    const gfx::VectorIcon& vector_id = requests[index]->GetIconId();
    icon->SetImage(
        gfx::CreateVectorIcon(vector_id, kIconSize, gfx::kChromeIconGrey));
    icon->SetTooltipText(base::string16());  // Redundant with the text fragment
    label_container->AddChildView(icon);
    views::Label* label =
        new views::Label(requests.at(index)->GetMessageTextFragment());
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_container->AddChildView(label);
    AddChildView(label_container);

    // Only show the toggle if every request wants to show it.
    show_persistence_toggle = show_persistence_toggle &&
                              requests[index]->ShouldShowPersistenceToggle();
  }

  if (show_persistence_toggle) {
    persist_checkbox_ = new views::Checkbox(
        l10n_util::GetStringUTF16(IDS_PERMISSIONS_BUBBLE_PERSIST_TEXT));
    persist_checkbox_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    persist_checkbox_->SetChecked(true);
    AddChildView(persist_checkbox_);
  }
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PERMISSIONS);
}

PermissionsBubbleDialogDelegateView::~PermissionsBubbleDialogDelegateView() {
  if (owner_)
    owner_->Closing();
}

void PermissionsBubbleDialogDelegateView::CloseBubble() {
  owner_ = nullptr;
  GetWidget()->Close();
}

ui::AXRole PermissionsBubbleDialogDelegateView::GetAccessibleWindowRole()
    const {
  return ui::AX_ROLE_ALERT_DIALOG;
}

bool PermissionsBubbleDialogDelegateView::ShouldShowCloseButton() const {
  return true;
}

base::string16 PermissionsBubbleDialogDelegateView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT,
                                    display_origin_);
}

void PermissionsBubbleDialogDelegateView::AddedToWidget() {
  std::unique_ptr<views::Label> title =
      views::BubbleFrameView::CreateDefaultTitleLabel(GetWindowTitle());
  title->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::BaseFont));
  GetBubbleFrameView()->SetTitleView(std::move(title));
}

void PermissionsBubbleDialogDelegateView::SizeToContents() {
  BubbleDialogDelegateView::SizeToContents();
}

void PermissionsBubbleDialogDelegateView::OnWidgetDestroying(
    views::Widget* widget) {
  views::BubbleDialogDelegateView::OnWidgetDestroying(widget);
  if (owner_) {
    owner_->Closing();
    owner_ = nullptr;
  }
}

int PermissionsBubbleDialogDelegateView::GetDefaultDialogButton() const {
  // To prevent permissions being accepted accidentally, and as a security
  // measure against crbug.com/619429, permission prompts should not be accepted
  // as the default action.
  return ui::DIALOG_BUTTON_NONE;
}

int PermissionsBubbleDialogDelegateView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

base::string16 PermissionsBubbleDialogDelegateView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return l10n_util::GetStringUTF16(IDS_PERMISSION_DENY);

  // The text differs based on whether OK is the only visible button.
  return l10n_util::GetStringUTF16(GetDialogButtons() == ui::DIALOG_BUTTON_OK
                                       ? IDS_OK
                                       : IDS_PERMISSION_ALLOW);
}

bool PermissionsBubbleDialogDelegateView::Cancel() {
  if (owner_) {
    owner_->TogglePersist(!persist_checkbox_ || persist_checkbox_->checked());
    owner_->Deny();
  }
  return true;
}

bool PermissionsBubbleDialogDelegateView::Accept() {
  if (owner_) {
    owner_->TogglePersist(!persist_checkbox_ || persist_checkbox_->checked());
    owner_->Accept();
  }
  return true;
}

bool PermissionsBubbleDialogDelegateView::Close() {
  // Neither explicit accept nor explicit deny.
  return true;
}

void PermissionsBubbleDialogDelegateView::UpdateAnchor() {
  views::View* anchor_view = GetPermissionAnchorView(owner_->browser());
  SetAnchorView(anchor_view);
  if (!anchor_view)
    SetAnchorRect(GetPermissionAnchorRect(owner_->browser()));
}

//////////////////////////////////////////////////////////////////////////////
// PermissionPromptImpl

PermissionPromptImpl::PermissionPromptImpl(Browser* browser, Delegate* delegate)
    : browser_(browser), delegate_(delegate), bubble_delegate_(nullptr) {
  Show();
}

PermissionPromptImpl::~PermissionPromptImpl() {
  if (bubble_delegate_)
    bubble_delegate_->CloseBubble();
}

bool PermissionPromptImpl::CanAcceptRequestUpdate() {
  return !(bubble_delegate_ && bubble_delegate_->IsMouseHovered());
}

void PermissionPromptImpl::UpdateAnchorPosition() {
  DCHECK(browser_);
  DCHECK(browser_->window());

  if (bubble_delegate_) {
    bubble_delegate_->set_parent_window(
        platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));
    bubble_delegate_->UpdateAnchor();
  }
}

gfx::NativeWindow PermissionPromptImpl::GetNativeWindow() {
  if (bubble_delegate_ && bubble_delegate_->GetWidget())
    return bubble_delegate_->GetWidget()->GetNativeWindow();
  return nullptr;
}

void PermissionPromptImpl::Closing() {
  if (bubble_delegate_)
    bubble_delegate_ = nullptr;
  if (delegate_)
    delegate_->Closing();
}

void PermissionPromptImpl::TogglePersist(bool value) {
  if (delegate_)
    delegate_->TogglePersist(value);
}

void PermissionPromptImpl::Accept() {
  if (delegate_)
    delegate_->Accept();
}

void PermissionPromptImpl::Deny() {
  if (delegate_)
    delegate_->Deny();
}

void PermissionPromptImpl::Show() {
  DCHECK(browser_);
  DCHECK(browser_->window());

  bubble_delegate_ =
      new PermissionsBubbleDialogDelegateView(this, delegate_->Requests());

  // Set |parent_window| because some valid anchors can become hidden.
  bubble_delegate_->set_parent_window(
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));

  // Compensate for vertical padding in the anchor view's image. Note this is
  // ignored whenever the anchor view is null.
  bubble_delegate_->set_anchor_view_insets(gfx::Insets(
      GetLayoutConstant(LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET), 0));

  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble_delegate_);
  // If a browser window (or popup) other than the bubble parent has focus,
  // don't take focus.
  if (browser_->window()->IsActive())
    widget->Show();
  else
    widget->ShowInactive();

  bubble_delegate_->SizeToContents();

  bubble_delegate_->UpdateAnchor();
}
