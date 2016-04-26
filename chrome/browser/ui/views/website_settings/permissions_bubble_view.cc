// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/permissions_bubble_view.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_view.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_view_observer.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "grit/components_strings.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

// Spacing between major items should be 9px.
const int kItemMajorSpacing = 9;

// (Square) pixel size of icon.
const int kIconSize = 18;

}  // namespace

// This class is a MenuButton which is given a PermissionMenuModel. It
// shows the current checked item in the menu model, and notifies its listener
// about any updates to the state of the selection.
// TODO(gbillock): refactor PermissionMenuButton to work like this and re-use?
class PermissionCombobox : public views::MenuButton,
                           public views::MenuButtonListener {
 public:
  // Get notifications when the selection changes.
  class Listener {
   public:
    virtual void PermissionSelectionChanged(int index, bool allowed) = 0;
  };

  PermissionCombobox(Listener* listener,
                     int index,
                     const GURL& url,
                     ContentSetting setting);
  ~PermissionCombobox() override;

  int index() const { return index_; }

  void GetAccessibleState(ui::AXViewState* state) override;

  // MenuButtonListener:
  void OnMenuButtonClicked(views::MenuButton* source,
                           const gfx::Point& point,
                           const ui::Event* event) override;

  // Callback when a permission's setting is changed.
  void PermissionChanged(const WebsiteSettingsUI::PermissionInfo& permission);

 private:
  int index_;
  Listener* listener_;
  std::unique_ptr<PermissionMenuModel> model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

PermissionCombobox::PermissionCombobox(Listener* listener,
                                       int index,
                                       const GURL& url,
                                       ContentSetting setting)
    : MenuButton(base::string16(), this, true),
      index_(index),
      listener_(listener),
      model_(new PermissionMenuModel(
          url,
          setting,
          base::Bind(&PermissionCombobox::PermissionChanged,
                     base::Unretained(this)))) {
  SetText(model_->GetLabelAt(model_->GetIndexOfCommandId(setting)));
  SizeToPreferredSize();
}

PermissionCombobox::~PermissionCombobox() {}

void PermissionCombobox::GetAccessibleState(ui::AXViewState* state) {
  MenuButton::GetAccessibleState(state);
  state->value = GetText();
}

void PermissionCombobox::OnMenuButtonClicked(views::MenuButton* source,
                                             const gfx::Point& point,
                                             const ui::Event* event) {
  menu_runner_.reset(
      new views::MenuRunner(model_.get(), views::MenuRunner::HAS_MNEMONICS));

  gfx::Point p(point);
  p.Offset(-source->width(), 0);
  if (menu_runner_->RunMenuAt(source->GetWidget()->GetTopLevelWidget(),
                              this,
                              gfx::Rect(p, gfx::Size()),
                              views::MENU_ANCHOR_TOPLEFT,
                              ui::MENU_SOURCE_NONE) ==
      views::MenuRunner::MENU_DELETED) {
    return;
  }
}

void PermissionCombobox::PermissionChanged(
    const WebsiteSettingsUI::PermissionInfo& permission) {
  SetText(model_->GetLabelAt(model_->GetIndexOfCommandId(permission.setting)));
  SizeToPreferredSize();

  listener_->PermissionSelectionChanged(
      index_, permission.setting == CONTENT_SETTING_ALLOW);
}

///////////////////////////////////////////////////////////////////////////////
// View implementation for the permissions bubble.
class PermissionsBubbleDialogDelegateView
    : public views::BubbleDialogDelegateView,
      public PermissionCombobox::Listener {
 public:
  PermissionsBubbleDialogDelegateView(
      views::View* anchor_view,
      views::BubbleBorder::Arrow anchor_arrow,
      PermissionBubbleViewViews* owner,
      const std::vector<PermissionBubbleRequest*>& requests,
      const std::vector<bool>& accept_state);
  ~PermissionsBubbleDialogDelegateView() override;

  void CloseBubble();
  void SizeToContents();

  // BubbleDialogDelegateView:
  bool ShouldShowCloseButton() const override;
  const gfx::FontList& GetTitleFontList() const override;
  base::string16 GetWindowTitle() const override;
  void OnWidgetDestroying(views::Widget* widget) override;
  gfx::Size GetPreferredSize() const override;
  void GetAccessibleState(ui::AXViewState* state) override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;

  // PermissionCombobox::Listener:
  void PermissionSelectionChanged(int index, bool allowed) override;

  // Updates the anchor's arrow and view. Also repositions the bubble so it's
  // displayed in the correct location.
  void UpdateAnchor(views::View* anchor_view,
                    views::BubbleBorder::Arrow anchor_arrow);

 private:
  PermissionBubbleViewViews* owner_;
  bool multiple_requests_;
  base::string16 display_origin_;
  std::unique_ptr<PermissionMenuModel> menu_button_model_;
  std::vector<PermissionCombobox*> customize_comboboxes_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsBubbleDialogDelegateView);
};

PermissionsBubbleDialogDelegateView::PermissionsBubbleDialogDelegateView(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_arrow,
    PermissionBubbleViewViews* owner,
    const std::vector<PermissionBubbleRequest*>& requests,
    const std::vector<bool>& accept_state)
    : views::BubbleDialogDelegateView(anchor_view, anchor_arrow),
      owner_(owner),
      multiple_requests_(requests.size() > 1) {
  DCHECK(!requests.empty());

  set_close_on_deactivate(false);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                                        kItemMajorSpacing));

  display_origin_ = url_formatter::FormatUrlForSecurityDisplay(
      requests[0]->GetOrigin(),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  for (size_t index = 0; index < requests.size(); index++) {
    DCHECK(index < accept_state.size());
    // The row is laid out containing a leading-aligned label area and a
    // trailing column which will be filled if there are multiple permission
    // requests.
    views::View* row = new views::View();
    views::GridLayout* row_layout = new views::GridLayout(row);
    row->SetLayoutManager(row_layout);
    views::ColumnSet* columns = row_layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                       0, views::GridLayout::USE_PREF, 0, 0);
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                       100, views::GridLayout::USE_PREF, 0, 0);
    row_layout->StartRow(0, 0);

    views::View* label_container = new views::View();
    label_container->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, views::kCheckboxIndent, 0,
        views::kItemLabelSpacing));
    views::ImageView* icon = new views::ImageView();
    gfx::VectorIconId vector_id = requests[index]->GetVectorIconId();
    if (vector_id != gfx::VectorIconId::VECTOR_ICON_NONE) {
      icon->SetImage(
          gfx::CreateVectorIcon(vector_id, kIconSize, gfx::kChromeIconGrey));
    } else {
      icon->SetImage(bundle.GetImageSkiaNamed(requests.at(index)->GetIconId()));
      icon->SetImageSize(gfx::Size(kIconSize, kIconSize));
    }
    icon->SetTooltipText(base::string16());  // Redundant with the text fragment
    label_container->AddChildView(icon);
    views::Label* label =
        new views::Label(requests.at(index)->GetMessageTextFragment());
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_container->AddChildView(label);
    row_layout->AddView(label_container);

    if (requests.size() > 1) {
      PermissionCombobox* combobox = new PermissionCombobox(
          this, index, requests[index]->GetOrigin(),
          accept_state[index] ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
      row_layout->AddView(combobox);
      customize_comboboxes_.push_back(combobox);
    } else {
      row_layout->AddView(new views::View());
    }

    AddChildView(row);
  }
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

gfx::Size PermissionsBubbleDialogDelegateView::GetPreferredSize() const {
  // TODO(estade): bubbles should default to this width.
  const int kWidth = 320 - GetInsets().width();
  return gfx::Size(kWidth, GetHeightForWidth(kWidth));
}

void PermissionsBubbleDialogDelegateView::GetAccessibleState(
    ui::AXViewState* state) {
  views::BubbleDialogDelegateView::GetAccessibleState(state);
  state->role = ui::AX_ROLE_ALERT_DIALOG;
}

int PermissionsBubbleDialogDelegateView::GetDialogButtons() const {
  int buttons = ui::DIALOG_BUTTON_OK;
  if (!multiple_requests_)
    buttons |= ui::DIALOG_BUTTON_CANCEL;
  return buttons;
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
  if (owner_)
    owner_->Deny();
  return true;
}

bool PermissionsBubbleDialogDelegateView::Accept() {
  if (owner_)
    owner_->Accept();
  return true;
}

bool PermissionsBubbleDialogDelegateView::Close() {
  // Neither explicit accept nor explicit deny.
  return true;
}

void PermissionsBubbleDialogDelegateView::PermissionSelectionChanged(
    int index,
    bool allowed) {
  owner_->Toggle(index, allowed);
}

void PermissionsBubbleDialogDelegateView::UpdateAnchor(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_arrow) {
  if (GetAnchorView() == anchor_view && arrow() == anchor_arrow)
    return;

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
}

//////////////////////////////////////////////////////////////////////////////
// PermissionBubbleViewViews

PermissionBubbleViewViews::PermissionBubbleViewViews(Browser* browser)
    : browser_(browser),
      delegate_(nullptr),
      bubble_delegate_(nullptr) {
  DCHECK(browser);
}

PermissionBubbleViewViews::~PermissionBubbleViewViews() {
}

// static
std::unique_ptr<PermissionBubbleView> PermissionBubbleView::Create(
    Browser* browser) {
  return base::WrapUnique(new PermissionBubbleViewViews(browser));
}

views::View* PermissionBubbleViewViews::GetAnchorView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

  if (browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return browser_view->GetLocationBarView()->location_icon_view();

  if (browser_view->IsFullscreenBubbleVisible())
    return browser_view->exclusive_access_bubble()->GetView();

  return browser_view->top_container();
}

views::BubbleBorder::Arrow PermissionBubbleViewViews::GetAnchorArrow() {
  if (browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return views::BubbleBorder::TOP_LEFT;
  return views::BubbleBorder::NONE;
}

void PermissionBubbleViewViews::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void PermissionBubbleViewViews::Show(
    const std::vector<PermissionBubbleRequest*>& requests,
    const std::vector<bool>& values) {
  if (bubble_delegate_)
    bubble_delegate_->CloseBubble();

  bubble_delegate_ = new PermissionsBubbleDialogDelegateView(
      GetAnchorView(), GetAnchorArrow(), this, requests, values);

  // Set |parent_window| because some valid anchors can become hidden.
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  bubble_delegate_->set_parent_window(widget->GetNativeView());

  views::BubbleDialogDelegateView::CreateBubble(bubble_delegate_)->Show();
  bubble_delegate_->SizeToContents();
}

bool PermissionBubbleViewViews::CanAcceptRequestUpdate() {
  return !(bubble_delegate_ && bubble_delegate_->IsMouseHovered());
}

void PermissionBubbleViewViews::Hide() {
  if (bubble_delegate_) {
    bubble_delegate_->CloseBubble();
    bubble_delegate_ = nullptr;
  }
}

bool PermissionBubbleViewViews::IsVisible() {
  return bubble_delegate_ != nullptr;
}

void PermissionBubbleViewViews::UpdateAnchorPosition() {
  if (IsVisible())
    bubble_delegate_->UpdateAnchor(GetAnchorView(), GetAnchorArrow());
}

gfx::NativeWindow PermissionBubbleViewViews::GetNativeWindow() {
  if (bubble_delegate_ && bubble_delegate_->GetWidget())
    return bubble_delegate_->GetWidget()->GetNativeWindow();
  return nullptr;
}

void PermissionBubbleViewViews::Closing() {
  if (bubble_delegate_)
    bubble_delegate_ = nullptr;
  if (delegate_)
    delegate_->Closing();
}

void PermissionBubbleViewViews::Toggle(int index, bool value) {
  if (delegate_)
    delegate_->ToggleAccept(index, value);
}

void PermissionBubbleViewViews::Accept() {
  if (delegate_)
    delegate_->Accept();
}

void PermissionBubbleViewViews::Deny() {
  if (delegate_)
    delegate_->Deny();
}
