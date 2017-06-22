// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row_observer.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace {

// (Square) pixel size of icon.
const int kIconSize = 18;

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
  bool ShouldShowCloseButton() const override;
  const gfx::FontList& GetTitleFontList() const override;
  base::string16 GetWindowTitle() const override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  int GetDefaultDialogButton() const override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  void SizeToContents() override;

  // Updates the anchor's arrow and view. Also repositions the bubble so it's
  // displayed in the correct location.
  void UpdateAnchor(views::View* anchor_view,
                    const gfx::Point& anchor_point,
                    views::BubbleBorder::Arrow anchor_arrow);

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

bool PermissionsBubbleDialogDelegateView::ShouldShowCloseButton() const {
  return true;
}

const gfx::FontList& PermissionsBubbleDialogDelegateView::GetTitleFontList()
    const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFontList(ui::ResourceBundle::BaseFont);
}

base::string16 PermissionsBubbleDialogDelegateView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT,
                                    display_origin_);
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

void PermissionsBubbleDialogDelegateView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  views::BubbleDialogDelegateView::GetAccessibleNodeData(node_data);
  node_data->role = ui::AX_ROLE_ALERT_DIALOG;
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

void PermissionsBubbleDialogDelegateView::UpdateAnchor(
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    views::BubbleBorder::Arrow anchor_arrow) {
  set_arrow(anchor_arrow);

  // Update the border in the bubble: will either add or remove the arrow.
  views::BubbleFrameView* frame =
      views::BubbleDialogDelegateView::GetBubbleFrameView();
  views::BubbleBorder::Arrow adjusted_arrow = anchor_arrow;
  if (base::i18n::IsRTL())
    adjusted_arrow = views::BubbleBorder::horizontal_mirror(adjusted_arrow);
  frame->SetBubbleBorder(std::unique_ptr<views::BubbleBorder>(
      new views::BubbleBorder(adjusted_arrow, shadow(), color())));

  // Reposition the bubble based on the updated arrow and view.
  SetAnchorView(anchor_view);
  // The anchor rect is ignored unless |anchor_view| is nullptr.
  SetAnchorRect(gfx::Rect(anchor_point, gfx::Size()));
}

//////////////////////////////////////////////////////////////////////////////
// PermissionPromptImpl

PermissionPromptImpl::PermissionPromptImpl(Browser* browser)
    : browser_(browser),
      delegate_(nullptr),
      bubble_delegate_(nullptr) {}

PermissionPromptImpl::~PermissionPromptImpl() {
}

void PermissionPromptImpl::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void PermissionPromptImpl::Show() {
  DCHECK(browser_);
  DCHECK(browser_->window());

  if (bubble_delegate_)
    bubble_delegate_->CloseBubble();

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

  bubble_delegate_->UpdateAnchor(GetAnchorView(),
                                 GetAnchorPoint(),
                                 GetAnchorArrow());
}

bool PermissionPromptImpl::CanAcceptRequestUpdate() {
  return !(bubble_delegate_ && bubble_delegate_->IsMouseHovered());
}

bool PermissionPromptImpl::HidesAutomatically() {
  return false;
}

void PermissionPromptImpl::Hide() {
  if (bubble_delegate_) {
    bubble_delegate_->CloseBubble();
    bubble_delegate_ = nullptr;
  }
}

void PermissionPromptImpl::UpdateAnchorPosition() {
  DCHECK(browser_);
  DCHECK(browser_->window());

  if (bubble_delegate_) {
    bubble_delegate_->set_parent_window(
        platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));
    bubble_delegate_->UpdateAnchor(GetAnchorView(),
                                   GetAnchorPoint(),
                                   GetAnchorArrow());
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
