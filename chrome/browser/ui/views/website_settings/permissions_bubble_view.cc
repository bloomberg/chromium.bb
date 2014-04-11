// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/permissions_bubble_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace {

// Spacing constant for outer margin. This is added to the
// bubble margin itself to equalize the margins at 20px.
const int kBubbleOuterMargin = 12;

// Spacing between major items should be 10px.
const int kItemMajorSpacing = 10;

// Button border size, draws inside the spacing distance.
const int kButtonBorderSize = 2;

}  // namespace

// Model for an Allow/Block combobox control. Each combobox has a separate
// model, which remembers the index of the permission it is associated with.
// TODO(gbillock): use a variant of the PermissionSelectorView for this.
class PermissionComboboxModel : public ui::ComboboxModel {
 public:
  enum Item {
    STATE_ALLOW = 0,
    STATE_BLOCK = 1
  };

  PermissionComboboxModel(int index, Item default_item);
  virtual ~PermissionComboboxModel() {}

  virtual int GetItemCount() const OVERRIDE;
  virtual base::string16 GetItemAt(int index) OVERRIDE;
  virtual int GetDefaultIndex() const OVERRIDE;

  // Return the item index this combobox is the model for.
  int index() { return index_; }

 private:
  int index_;
  int default_item_;
};

PermissionComboboxModel::PermissionComboboxModel(int index, Item default_item)
    : index_(index), default_item_(default_item) {}

int PermissionComboboxModel::GetItemCount() const {
  return 2;  // 'allow' and 'block'
}

base::string16 PermissionComboboxModel::GetItemAt(int index) {
  if (index == 0)
    return l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW);
  else
    return l10n_util::GetStringUTF16(IDS_PERMISSION_DENY);
}

int PermissionComboboxModel::GetDefaultIndex() const {
  return default_item_;
}

class CustomizeDenyComboboxModel : public ui::ComboboxModel {
 public:
  enum Item {
    INDEX_DENY = 0,
    INDEX_CUSTOMIZE = 1
  };

  CustomizeDenyComboboxModel() {}
  virtual ~CustomizeDenyComboboxModel() {}

  virtual int GetItemCount() const OVERRIDE;
  virtual base::string16 GetItemAt(int index) OVERRIDE;
  virtual int GetDefaultIndex() const OVERRIDE;
};

int CustomizeDenyComboboxModel::GetItemCount() const {
  return 2;
}

base::string16 CustomizeDenyComboboxModel::GetItemAt(int index) {
  if (index == INDEX_DENY)
    return l10n_util::GetStringUTF16(IDS_PERMISSION_DENY);
  else
    return l10n_util::GetStringUTF16(IDS_PERMISSION_CUSTOMIZE);
}

int CustomizeDenyComboboxModel::GetDefaultIndex() const {
  return INDEX_DENY;
}

///////////////////////////////////////////////////////////////////////////////
// View implementation for the permissions bubble.

class PermissionsBubbleDelegateView : public views::BubbleDelegateView,
                                      public views::ButtonListener,
                                      public views::ComboboxListener {
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
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // ButtonListener:
  virtual void ButtonPressed(views::Button* button,
                             const ui::Event& event) OVERRIDE;

  // ComboboxListener:
  virtual void OnPerformAction(views::Combobox* combobox) OVERRIDE;

 private:
  PermissionBubbleViewViews* owner_;
  views::Button* allow_;
  views::Button* deny_;
  views::Combobox* deny_combobox_;
  base::string16 title_;
  std::string hostname_;

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
      deny_combobox_(NULL) {
  DCHECK(!requests.empty());

  RemoveAllChildViews(true);
  set_close_on_esc(false);
  set_close_on_deactivate(false);

  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, kBubbleOuterMargin, 0, kItemMajorSpacing));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // TODO(gbillock): account for different requests from different hosts.
  hostname_ = requests[0]->GetRequestingHostname().host();

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
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 5));
    views::ImageView* icon = new views::ImageView();
    icon->SetImage(bundle.GetImageSkiaNamed(requests.at(index)->GetIconID()));
    label_container->AddChildView(icon);
    views::Label* label =
        new views::Label(requests.at(index)->GetMessageTextFragment());
    label->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumFont));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_container->AddChildView(label);
    row_layout->AddView(label_container);

    if (customization_mode) {
      views::Combobox* combobox = new views::Combobox(
          new PermissionComboboxModel(index,
              accept_state[index] ? PermissionComboboxModel::STATE_ALLOW
                                  : PermissionComboboxModel::STATE_BLOCK));
      combobox->set_listener(this);
      row_layout->AddView(combobox);
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

  base::string16 allow_text = l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW);
  views::LabelButton* allow_button = new views::LabelButton(this, allow_text);
  allow_button->SetStyle(views::Button::STYLE_BUTTON);
  allow_button->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumFont));
  button_layout->AddView(allow_button);
  allow_ = allow_button;

  // Deny button is a regular button when there's only one option, and a
  // STYLE_ACTION Combobox when there are more than one option and
  // customization is an option.
  base::string16 deny_text = l10n_util::GetStringUTF16(IDS_PERMISSION_DENY);
  if (requests.size() == 1) {
    views::LabelButton* deny_button = new views::LabelButton(this, deny_text);
    deny_button->SetStyle(views::Button::STYLE_BUTTON);
    deny_button->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumFont));
    button_layout->AddView(deny_button);
    deny_ = deny_button;
  } else {
    views::Combobox* deny_combobox = new views::Combobox(
        new CustomizeDenyComboboxModel());
    deny_combobox->set_listener(this);
    deny_combobox->SetStyle(views::Combobox::STYLE_ACTION);
    button_layout->AddView(deny_combobox);
    deny_combobox_ = deny_combobox;
  }

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

base::string16 PermissionsBubbleDelegateView::GetWindowTitle() const {
  if (!title_.empty()) {
    return title_;
  }

  return l10n_util::GetStringFUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT,
                                    base::UTF8ToUTF16(hostname_));
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

void PermissionsBubbleDelegateView::OnPerformAction(
    views::Combobox* combobox) {
  if (combobox == deny_combobox_) {
    if (combobox->selected_index() ==
        CustomizeDenyComboboxModel::INDEX_CUSTOMIZE)
      owner_->SetCustomizationMode();
    else if (combobox->selected_index() ==
             CustomizeDenyComboboxModel::INDEX_DENY)
      owner_->Deny();
    return;
  }

  PermissionComboboxModel* model =
      static_cast<PermissionComboboxModel*>(combobox->model());
  owner_->Toggle(model->index(), combobox->selected_index() == 0);
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
