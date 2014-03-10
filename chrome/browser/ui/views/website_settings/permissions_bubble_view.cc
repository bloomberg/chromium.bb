// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/permissions_bubble_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
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

// View implementation for the permissions bubble.
class PermissionsBubbleDelegateView : public views::BubbleDelegateView,
                                      public views::ButtonListener {
 public:
  explicit PermissionsBubbleDelegateView(
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

 private:
  PermissionBubbleViewViews* owner_;
  views::Button* allow_;
  views::Button* deny_;
  views::Button* customize_;
  base::string16 title_;

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
      customize_(NULL) {
  RemoveAllChildViews(true);
  set_close_on_esc(false);
  set_close_on_deactivate(false);

  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, kBubbleOuterMargin, 0, kItemMajorSpacing));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  if (requests.size() == 1) {
    title_ = requests[0]->GetMessageText();
    if (GetWidget())
      GetWidget()->UpdateWindowTitle();
  } else {
    for (std::vector<PermissionBubbleRequest*>::const_iterator it =
             requests.begin();
         it != requests.end(); it++) {
      if (customization_mode) {
        views::Checkbox* label =
            new views::Checkbox((*it)->GetMessageTextFragment());
        label->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumFont));
        label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        AddChildView(label);
      } else {
        // TODO(gbillock): i18n the bullet string?
        const base::string16 bullet = base::UTF8ToUTF16(" • ");
        views::Label* label =
            new views::Label(bullet + (*it)->GetMessageTextFragment());
        label->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumFont));
        label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        AddChildView(label);
      }
    }
  }

  views::View* button_row = new views::View();
  views::GridLayout* button_layout = new views::GridLayout(button_row);
  views::ColumnSet* columns = button_layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                     100.0, views::GridLayout::USE_PREF, 0, 0);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                     0, views::GridLayout::USE_PREF, 0, 0);
  if (!customization_mode) {
    columns->AddPaddingColumn(0, kItemMajorSpacing - (2*kButtonBorderSize));
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL,
                       0, views::GridLayout::USE_PREF, 0, 0);
  }

  button_row->SetLayoutManager(button_layout);
  AddChildView(button_row);

  button_layout->StartRow(0, 0);

  // Customization case: empty "Customize" label and an "OK" button
  if (customization_mode) {
    button_layout->AddView(new views::View());

    views::LabelButton* ok_button =
        new views::LabelButton(this, l10n_util::GetStringUTF16(
            IDS_OK));
    ok_button->SetStyle(views::Button::STYLE_BUTTON);
    button_layout->AddView(ok_button);
    allow_ = ok_button;

    button_layout->AddPaddingRow(0, kBubbleOuterMargin);
    return;
  }

  // Only show the "Customize" button if there is more than one option.
  if (requests.size() > 1) {
    views::LabelButton* customize = new views::LabelButton(this,
        l10n_util::GetStringUTF16(IDS_PERMISSION_CUSTOMIZE));
    customize->SetStyle(views::Button::STYLE_TEXTBUTTON);
    customize->SetTextColor(views::Button::STATE_NORMAL, SK_ColorBLUE);
    customize->SetTextColor(views::Button::STATE_HOVERED, SK_ColorBLUE);
    customize->SetTextColor(views::Button::STATE_PRESSED, SK_ColorBLUE);
    button_layout->AddView(customize);
  } else {
    button_layout->AddView(new views::View());
  }

  // Lay out the Deny/Allow buttons.
  base::string16 deny_text = l10n_util::GetStringUTF16(IDS_PERMISSION_DENY);
  views::LabelButton* deny_button = new views::LabelButton(this, deny_text);
  deny_button->SetStyle(views::Button::STYLE_BUTTON);
  deny_button->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumFont));
  button_layout->AddView(deny_button);
  deny_ = deny_button;

  base::string16 allow_text = l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW);
  views::LabelButton* allow_button = new views::LabelButton(this, allow_text);
  allow_button->SetStyle(views::Button::STYLE_BUTTON);
  allow_button->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumFont));
  button_layout->AddView(allow_button);
  allow_ = allow_button;

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

  return l10n_util::GetStringUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT);
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
  else if (button == customize_)
    owner_->SetCustomizationMode();
}

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

  PermissionsBubbleDelegateView* bubble_delegate =
      new PermissionsBubbleDelegateView(anchor_view_, this,
                                        requests, values, customization_mode);
  bubble_delegate_ = bubble_delegate;
  views::BubbleDelegateView::CreateBubble(bubble_delegate_);

  bubble_delegate_->StartFade(true);
  bubble_delegate->SizeToContents();
}

bool PermissionBubbleViewViews::CanAcceptRequestUpdate() {
  // TODO(gbillock): support this.
  // return bubble_delegate_ && bubble_delegate_->IsMouseHovered();
  return false;
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
