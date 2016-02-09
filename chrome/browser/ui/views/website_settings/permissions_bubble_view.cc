// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/permissions_bubble_view.h"

#include <stddef.h>

#include "base/macros.h"
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
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
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
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace {

// Spacing between major items should be 9px.
const int kItemMajorSpacing = 9;

// Button border size, draws inside the spacing distance.
const int kButtonBorderSize = 2;

// (Square) pixel size of icon.
const int kIconSize = 18;

// Number of pixels to indent the permission request labels.
const int kPermissionIndentSpacing = 12;

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
  void OnMenuButtonClicked(View* source, const gfx::Point& point) override;

  // Callback when a permission's setting is changed.
  void PermissionChanged(const WebsiteSettingsUI::PermissionInfo& permission);

 private:
  int index_;
  Listener* listener_;
  scoped_ptr<PermissionMenuModel> model_;
  scoped_ptr<views::MenuRunner> menu_runner_;
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

void PermissionCombobox::OnMenuButtonClicked(View* source,
                                             const gfx::Point& point) {
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
class PermissionsBubbleDelegateView : public views::BubbleDelegateView,
                                      public views::ButtonListener,
                                      public PermissionCombobox::Listener {
 public:
  PermissionsBubbleDelegateView(
      views::View* anchor_view,
      views::BubbleBorder::Arrow anchor_arrow,
      PermissionBubbleViewViews* owner,
      const std::string& languages,
      const std::vector<PermissionBubbleRequest*>& requests,
      const std::vector<bool>& accept_state);
  ~PermissionsBubbleDelegateView() override;

  void Close();
  void SizeToContents();

  // BubbleDelegateView:
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowTitle() const override;
  const gfx::FontList& GetTitleFontList() const override;
  base::string16 GetWindowTitle() const override;
  void OnWidgetDestroying(views::Widget* widget) override;
  gfx::Size GetPreferredSize() const override;
  void GetAccessibleState(ui::AXViewState* state) override;

  // ButtonListener:
  void ButtonPressed(views::Button* button, const ui::Event& event) override;

  // PermissionCombobox::Listener:
  void PermissionSelectionChanged(int index, bool allowed) override;

  // Updates the anchor's arrow and view. Also repositions the bubble so it's
  // displayed in the correct location.
  void UpdateAnchor(views::View* anchor_view,
                    views::BubbleBorder::Arrow anchor_arrow);

 private:
  PermissionBubbleViewViews* owner_;
  views::Button* allow_;
  views::Button* deny_;
  base::string16 display_origin_;
  scoped_ptr<PermissionMenuModel> menu_button_model_;
  std::vector<PermissionCombobox*> customize_comboboxes_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsBubbleDelegateView);
};

PermissionsBubbleDelegateView::PermissionsBubbleDelegateView(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_arrow,
    PermissionBubbleViewViews* owner,
    const std::string& languages,
    const std::vector<PermissionBubbleRequest*>& requests,
    const std::vector<bool>& accept_state)
    : views::BubbleDelegateView(anchor_view, anchor_arrow),
      owner_(owner),
      allow_(nullptr),
      deny_(nullptr) {
  DCHECK(!requests.empty());

  set_close_on_esc(true);
  set_close_on_deactivate(false);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                                        kItemMajorSpacing));

  display_origin_ = url_formatter::FormatUrlForSecurityDisplay(
      requests[0]->GetOrigin(), languages);

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
        views::BoxLayout::kHorizontal, kPermissionIndentSpacing, 0, 0));
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

  views::View* button_row = new views::View();
  views::GridLayout* button_layout = new views::GridLayout(button_row);
  views::ColumnSet* columns = button_layout->AddColumnSet(0);
  button_row->SetLayoutManager(button_layout);
  AddChildView(button_row);

  // For multiple permissions: just an "OK" button.
  if (requests.size() > 1) {
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                       100, views::GridLayout::USE_PREF, 0, 0);
    button_layout->StartRowWithPadding(0, 0, 0, 4);
    views::LabelButton* ok_button =
        new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_OK));
    ok_button->SetStyle(views::Button::STYLE_BUTTON);
    button_layout->AddView(ok_button);
    allow_ = ok_button;
    return;
  }

  // For a single permission: lay out the Deny/Allow buttons.
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                     100, views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, kItemMajorSpacing - (2*kButtonBorderSize));
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                     0, views::GridLayout::USE_PREF, 0, 0);
  button_layout->StartRow(0, 0);

  base::string16 allow_text = l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW);
  views::LabelButton* allow_button = new views::LabelButton(this, allow_text);
  allow_button->SetStyle(views::Button::STYLE_BUTTON);
  button_layout->AddView(allow_button);
  allow_ = allow_button;

  base::string16 deny_text = l10n_util::GetStringUTF16(IDS_PERMISSION_DENY);
  views::LabelButton* deny_button = new views::LabelButton(this, deny_text);
  deny_button->SetStyle(views::Button::STYLE_BUTTON);
  button_layout->AddView(deny_button);
  deny_ = deny_button;
}

PermissionsBubbleDelegateView::~PermissionsBubbleDelegateView() {
  if (owner_)
    owner_->Closing();
}

void PermissionsBubbleDelegateView::Close() {
  owner_ = nullptr;
  GetWidget()->Close();
}

bool PermissionsBubbleDelegateView::ShouldShowCloseButton() const {
  return true;
}

bool PermissionsBubbleDelegateView::ShouldShowWindowTitle() const {
  return true;
}

const gfx::FontList& PermissionsBubbleDelegateView::GetTitleFontList() const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFontList(ui::ResourceBundle::BaseFont);
}

base::string16 PermissionsBubbleDelegateView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT,
                                    display_origin_);
}

void PermissionsBubbleDelegateView::SizeToContents() {
  BubbleDelegateView::SizeToContents();
}

void PermissionsBubbleDelegateView::OnWidgetDestroying(views::Widget* widget) {
  views::BubbleDelegateView::OnWidgetDestroying(widget);
  if (owner_) {
    owner_->Closing();
    owner_ = nullptr;
  }
}

gfx::Size PermissionsBubbleDelegateView::GetPreferredSize() const {
  // TODO(estade): bubbles should default to this width.
  const int kWidth = 320 - GetInsets().width();
  return gfx::Size(kWidth, GetHeightForWidth(kWidth));
}

void PermissionsBubbleDelegateView::GetAccessibleState(ui::AXViewState* state) {
  views::BubbleDelegateView::GetAccessibleState(state);
  state->role = ui::AX_ROLE_ALERT_DIALOG;
}

void PermissionsBubbleDelegateView::ButtonPressed(views::Button* button,
                                                  const ui::Event& event) {
  if (!owner_)
    return;

  if (button == allow_)
    owner_->Accept();
  else if (button == deny_)
    owner_->Deny();
}

void PermissionsBubbleDelegateView::PermissionSelectionChanged(
    int index, bool allowed) {
  owner_->Toggle(index, allowed);
}

void PermissionsBubbleDelegateView::UpdateAnchor(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_arrow) {
  if (GetAnchorView() == anchor_view && arrow() == anchor_arrow)
    return;

  set_arrow(anchor_arrow);

  // Update the border in the bubble: will either add or remove the arrow.
  views::BubbleFrameView* frame =
      views::BubbleDelegateView::GetBubbleFrameView();
  views::BubbleBorder::Arrow adjusted_arrow = anchor_arrow;
  if (base::i18n::IsRTL())
    adjusted_arrow = views::BubbleBorder::horizontal_mirror(adjusted_arrow);
  frame->SetBubbleBorder(scoped_ptr<views::BubbleBorder>(
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
scoped_ptr<PermissionBubbleView> PermissionBubbleView::Create(
    Browser* browser) {
  return make_scoped_ptr(new PermissionBubbleViewViews(browser));
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
    bubble_delegate_->Close();

  bubble_delegate_ =
      new PermissionsBubbleDelegateView(
          GetAnchorView(), GetAnchorArrow(), this,
          browser_->profile()->GetPrefs()->GetString(prefs::kAcceptLanguages),
          requests, values);

  // Set |parent_window| because some valid anchors can become hidden.
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  bubble_delegate_->set_parent_window(widget->GetNativeView());

  views::BubbleDelegateView::CreateBubble(bubble_delegate_)->Show();
  bubble_delegate_->SizeToContents();
}

bool PermissionBubbleViewViews::CanAcceptRequestUpdate() {
  return !(bubble_delegate_ && bubble_delegate_->IsMouseHovered());
}

void PermissionBubbleViewViews::Hide() {
  if (bubble_delegate_) {
    bubble_delegate_->Close();
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
