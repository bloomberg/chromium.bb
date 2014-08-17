// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/permissions_bubble_view.h"

#include "base/strings/string16.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_view.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_view_observer.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_constants.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/bubble/bubble_delegate.h"
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

// Spacing constant for outer margin. This is added to the
// bubble margin itself to equalize the margins at 13px.
const int kBubbleOuterMargin = 5;

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
// TODO: refactor PermissionMenuButton to work like this and re-use?
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
  virtual ~PermissionCombobox();

  int index() const { return index_; }

  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;

  // MenuButtonListener:
  virtual void OnMenuButtonClicked(View* source,
                                   const gfx::Point& point) OVERRIDE;

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
    : MenuButton(NULL, base::string16(), this, true),
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

// A combobox originating on the Allow button allowing for customization
// of permissions.
class CustomizeAllowComboboxModel : public ui::ComboboxModel {
 public:
  enum Item {
    INDEX_ALLOW = 0,
    INDEX_CUSTOMIZE = 1
  };

  CustomizeAllowComboboxModel() {}
  virtual ~CustomizeAllowComboboxModel() {}

  virtual int GetItemCount() const OVERRIDE;
  virtual base::string16 GetItemAt(int index) OVERRIDE;
  virtual int GetDefaultIndex() const OVERRIDE;
};

int CustomizeAllowComboboxModel::GetItemCount() const {
  return 2;
}

base::string16 CustomizeAllowComboboxModel::GetItemAt(int index) {
  if (index == INDEX_ALLOW)
    return l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW);
  else
    return l10n_util::GetStringUTF16(IDS_PERMISSION_CUSTOMIZE);
}

int CustomizeAllowComboboxModel::GetDefaultIndex() const {
  return INDEX_ALLOW;
}

///////////////////////////////////////////////////////////////////////////////
// View implementation for the permissions bubble.
class PermissionsBubbleDelegateView : public views::BubbleDelegateView,
                                      public views::ButtonListener,
                                      public views::ComboboxListener,
                                      public PermissionCombobox::Listener {
 public:
  PermissionsBubbleDelegateView(
      views::View* anchor,
      PermissionBubbleViewViews* owner,
      const std::vector<PermissionBubbleRequest*>& requests,
      const std::vector<bool>& accept_state,
      bool customization_mode);
  virtual ~PermissionsBubbleDelegateView();

  void Close();
  void SizeToContents();

  // BubbleDelegateView:
  virtual bool ShouldShowCloseButton() const OVERRIDE;
  virtual bool ShouldShowWindowTitle() const OVERRIDE;
  virtual const gfx::FontList& GetTitleFontList() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // ButtonListener:
  virtual void ButtonPressed(views::Button* button,
                             const ui::Event& event) OVERRIDE;

  // ComboboxListener:
  virtual void OnPerformAction(views::Combobox* combobox) OVERRIDE;

  // PermissionCombobox::Listener:
  virtual void PermissionSelectionChanged(int index, bool allowed) OVERRIDE;

 private:
  PermissionBubbleViewViews* owner_;
  views::Button* allow_;
  views::Button* deny_;
  views::Combobox* allow_combobox_;
  base::string16 hostname_;
  scoped_ptr<PermissionMenuModel> menu_button_model_;
  std::vector<PermissionCombobox*> customize_comboboxes_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsBubbleDelegateView);
};

PermissionsBubbleDelegateView::PermissionsBubbleDelegateView(
    views::View* anchor,
    PermissionBubbleViewViews* owner,
    const std::vector<PermissionBubbleRequest*>& requests,
    const std::vector<bool>& accept_state,
    bool customization_mode)
    : views::BubbleDelegateView(anchor, views::BubbleBorder::TOP_LEFT),
      owner_(owner),
      allow_(NULL),
      deny_(NULL),
      allow_combobox_(NULL) {
  DCHECK(!requests.empty());

  RemoveAllChildViews(true);
  customize_comboboxes_.clear();
  set_close_on_esc(false);
  set_close_on_deactivate(false);

  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, kBubbleOuterMargin, 0, kItemMajorSpacing));

  // TODO(gbillock): support other languages than English.
  hostname_ = net::FormatUrl(requests[0]->GetRequestingHostname(),
                             "en",
                             net::kFormatUrlOmitUsernamePassword |
                             net::kFormatUrlOmitTrailingSlashOnBareHostname,
                             net::UnescapeRule::SPACES, NULL, NULL, NULL);

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  for (size_t index = 0; index < requests.size(); index++) {
    DCHECK(index < accept_state.size());
    // The row is laid out containing a leading-aligned label area and a
    // trailing column which will be filled during customization with a
    // combobox.
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
    label_container->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal,
                             kPermissionIndentSpacing,
                             0, kBubbleOuterMargin));
    views::ImageView* icon = new views::ImageView();
    icon->SetImage(bundle.GetImageSkiaNamed(requests.at(index)->GetIconID()));
    icon->SetImageSize(gfx::Size(kIconSize, kIconSize));
    label_container->AddChildView(icon);
    views::Label* label =
        new views::Label(requests.at(index)->GetMessageTextFragment());
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_container->AddChildView(label);
    row_layout->AddView(label_container);

    if (customization_mode) {
      PermissionCombobox* combobox = new PermissionCombobox(
          this,
          index,
          requests[index]->GetRequestingHostname(),
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

  // Customization case: just an "OK" button
  if (customization_mode) {
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                       100, views::GridLayout::USE_PREF, 0, 0);
    button_layout->StartRow(0, 0);
    views::LabelButton* ok_button =
        new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_OK));
    ok_button->SetStyle(views::Button::STYLE_BUTTON);
    button_layout->AddView(ok_button);
    allow_ = ok_button;

    button_layout->AddPaddingRow(0, kBubbleOuterMargin);
    return;
  }

  // No customization: lay out the Deny/Allow buttons.

  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                     100, views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, kItemMajorSpacing - (2*kButtonBorderSize));
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                     0, views::GridLayout::USE_PREF, 0, 0);
  button_layout->StartRow(0, 0);

  // Allow button is a regular button when there's only one option, and a
  // STYLE_ACTION Combobox when there are more than one option and
  // customization is an option.

  base::string16 allow_text = l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW);
  if (requests.size() == 1) {
    views::LabelButton* allow_button = new views::LabelButton(this, allow_text);
    allow_button->SetStyle(views::Button::STYLE_BUTTON);
    button_layout->AddView(allow_button);
    allow_ = allow_button;
  } else {
    views::Combobox* allow_combobox = new views::Combobox(
        new CustomizeAllowComboboxModel());
    allow_combobox->set_listener(this);
    allow_combobox->SetStyle(views::Combobox::STYLE_ACTION);
    button_layout->AddView(allow_combobox);
    allow_combobox_ = allow_combobox;
  }

  base::string16 deny_text = l10n_util::GetStringUTF16(IDS_PERMISSION_DENY);
  views::LabelButton* deny_button = new views::LabelButton(this, deny_text);
  deny_button->SetStyle(views::Button::STYLE_BUTTON);
  button_layout->AddView(deny_button);
  deny_ = deny_button;

  button_layout->AddPaddingRow(0, kBubbleOuterMargin);
}

PermissionsBubbleDelegateView::~PermissionsBubbleDelegateView() {
  if (owner_)
    owner_->Closing();
}

void PermissionsBubbleDelegateView::Close() {
  owner_ = NULL;
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
                                    hostname_);
}

void PermissionsBubbleDelegateView::SizeToContents() {
  BubbleDelegateView::SizeToContents();
}

void PermissionsBubbleDelegateView::OnWidgetDestroying(views::Widget* widget) {
  views::BubbleDelegateView::OnWidgetDestroying(widget);
  if (owner_) {
    owner_->Closing();
    owner_ = NULL;
  }
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

void PermissionsBubbleDelegateView::OnPerformAction(
    views::Combobox* combobox) {
  if (combobox == allow_combobox_) {
    if (combobox->selected_index() ==
        CustomizeAllowComboboxModel::INDEX_CUSTOMIZE)
      owner_->SetCustomizationMode();
    else if (combobox->selected_index() ==
             CustomizeAllowComboboxModel::INDEX_ALLOW)
      owner_->Accept();
  }
}

//////////////////////////////////////////////////////////////////////////////
// PermissionBubbleViewViews

PermissionBubbleViewViews::PermissionBubbleViewViews(views::View* anchor_view)
    : anchor_view_(anchor_view),
      delegate_(NULL),
      bubble_delegate_(NULL) {}

PermissionBubbleViewViews::~PermissionBubbleViewViews() {
  if (delegate_)
    delegate_->SetView(NULL);
}

void PermissionBubbleViewViews::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void PermissionBubbleViewViews::Show(
    const std::vector<PermissionBubbleRequest*>& requests,
    const std::vector<bool>& values,
    bool customization_mode) {
  if (bubble_delegate_ != NULL)
    bubble_delegate_->Close();

  bubble_delegate_ =
      new PermissionsBubbleDelegateView(anchor_view_, this,
                                        requests, values, customization_mode);
  views::BubbleDelegateView::CreateBubble(bubble_delegate_)->Show();
  bubble_delegate_->SizeToContents();
}

bool PermissionBubbleViewViews::CanAcceptRequestUpdate() {
  return !(bubble_delegate_ && bubble_delegate_->IsMouseHovered());
}

void PermissionBubbleViewViews::Hide() {
  if (bubble_delegate_) {
    bubble_delegate_->Close();
    bubble_delegate_ = NULL;
  }
}

bool PermissionBubbleViewViews::IsVisible() {
  return bubble_delegate_ != NULL;
}

void PermissionBubbleViewViews::Closing() {
  if (bubble_delegate_)
    bubble_delegate_ = NULL;
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

void PermissionBubbleViewViews::SetCustomizationMode() {
  if (delegate_)
    delegate_->SetCustomizationMode();
}
