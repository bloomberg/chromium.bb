// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_to_mobile_bubble_view.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_to_mobile_service.h"
#include "chrome/browser/chrome_to_mobile_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_style.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using views::GridLayout;

namespace {

// The title label's color; matches the bookmark bubble's title.
const SkColor kTitleColor = 0xFF062D75;

// The millisecond duration of the "Sending..." progress throb animation.
const size_t kProgressThrobDurationMS = 2400;

// The seconds to delay before automatically closing the bubble after sending.
const int kAutoCloseDelay = 3;

// Downcast TextButton |view| and set the icon image with the resource |id|.
void SetTextButtonIconToId(views::View* view, int id) {
  static_cast<views::TextButton*>(view)->
      SetIcon(*ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id));
}

}  // namespace

// Declared in browser_dialogs.h so callers don't have to depend on our header.

namespace chrome {

void ShowChromeToMobileBubbleView(views::View* anchor_view, Browser* browser) {
  ChromeToMobileBubbleView::ShowBubble(anchor_view, browser);
}

void HideChromeToMobileBubbleView() {
  ChromeToMobileBubbleView::Hide();
}

bool IsChromeToMobileBubbleViewShowing() {
  return ChromeToMobileBubbleView::IsShowing();
}

}  // namespace chrome

// ChromeToMobileBubbleView ----------------------------------------------------

ChromeToMobileBubbleView* ChromeToMobileBubbleView::bubble_ = NULL;

ChromeToMobileBubbleView::~ChromeToMobileBubbleView() {}

// static
void ChromeToMobileBubbleView::ShowBubble(views::View* anchor_view,
                                          Browser* browser) {
  if (IsShowing())
    return;

  // Show the lit mobile device icon during the bubble's lifetime.
  SetTextButtonIconToId(anchor_view, IDR_MOBILE_LIT);
  bubble_ = new ChromeToMobileBubbleView(anchor_view, browser);
  views::BubbleDelegateView::CreateBubble(bubble_);
  bubble_->Show();
}

// static
bool ChromeToMobileBubbleView::IsShowing() {
  return bubble_ != NULL;
}

void ChromeToMobileBubbleView::Hide() {
  if (IsShowing())
    bubble_->GetWidget()->Close();
}

views::View* ChromeToMobileBubbleView::GetInitiallyFocusedView() {
  return send_;
}

void ChromeToMobileBubbleView::WindowClosing() {
  // We have to reset |bubble_| here, not in our destructor, because we'll be
  // destroyed asynchronously and the shown state will be checked before then.
  DCHECK_EQ(bubble_, this);
  bubble_ = NULL;

  // Instruct the service to delete the snapshot file.
  service_->DeleteSnapshot(snapshot_path_);

  // Restore the resting state action box icon.
  SetTextButtonIconToId(anchor_view(), IDR_ACTION_BOX_BUTTON_NORMAL);
}

bool ChromeToMobileBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_RETURN &&
      (send_->HasFocus() || cancel_->HasFocus())) {
    HandleButtonPressed(send_->HasFocus() ? send_ : cancel_);
    return true;
  }
  return BubbleDelegateView::AcceleratorPressed(accelerator);
}

void ChromeToMobileBubbleView::AnimationProgressed(
    const ui::Animation* animation) {
  if (animation == progress_animation_.get()) {
    double animation_value = animation->GetCurrentValue();
    int message = IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_3;
    // Show each of four messages for 1/4 of the animation.
    if (animation_value < 0.25)
      message = IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_0;
    else if (animation_value < 0.5)
      message = IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_1;
    else if (animation_value < 0.75)
      message = IDS_CHROME_TO_MOBILE_BUBBLE_SENDING_2;
    send_->SetText(l10n_util::GetStringUTF16(message));
    // Run Layout but do not resize the bubble for each progress message.
    Layout();
    return;
  }
  views::BubbleDelegateView::AnimationProgressed(animation);
}

void ChromeToMobileBubbleView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  HandleButtonPressed(sender);
}

void ChromeToMobileBubbleView::SnapshotGenerated(const base::FilePath& path,
                                                 int64 bytes) {
  snapshot_path_ = path;
  if (bytes > 0) {
    send_copy_->SetText(l10n_util::GetStringFUTF16(
        IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY, ui::FormatBytes(bytes)));
    send_copy_->SetEnabled(true);
  } else {
    send_copy_->SetText(l10n_util::GetStringUTF16(
        IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY_FAILED));
  }
  Layout();
}

void ChromeToMobileBubbleView::OnSendComplete(bool success) {
  progress_animation_->Stop();
  send_->set_alignment(views::TextButtonBase::ALIGN_CENTER);

  if (success) {
    send_->SetText(l10n_util::GetStringUTF16(IDS_CHROME_TO_MOBILE_BUBBLE_SENT));
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(&ChromeToMobileBubbleView::Hide),
        base::TimeDelta::FromSeconds(kAutoCloseDelay));
  } else {
    send_->SetText(
        l10n_util::GetStringUTF16(IDS_CHROME_TO_MOBILE_BUBBLE_ERROR));
    views::Label* error_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_CHROME_TO_MOBILE_BUBBLE_ERROR_MESSAGE));
    error_label->SetEnabledColor(SK_ColorRED);
    GridLayout* layout = static_cast<GridLayout*>(GetLayoutManager());
    layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
    layout->StartRow(0, 0 /*kSingleColumnSetId*/);
    layout->AddView(error_label);
    SizeToContents();
  }

  Layout();
}

void ChromeToMobileBubbleView::Init() {
  DCHECK(service_->HasMobiles());

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  const size_t kSingleColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kSingleColumnSetId);
  cs->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  const size_t kRadioColumnSetId = 1;
  cs = layout->AddColumnSet(kRadioColumnSetId);
  cs->AddPaddingColumn(0, chrome_style::kCheckboxIndent);
  cs->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                GridLayout::USE_PREF, 0, 0);

  const size_t kButtonColumnSetId = 2;
  cs = layout->AddColumnSet(kButtonColumnSetId);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);
  // Subtract 2px for the natural button padding and to correspond with row
  // separation height; like BookmarkBubbleView.
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing - 2);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  views::Label* title_label = new views::Label();
  title_label->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  title_label->SetEnabledColor(kTitleColor);
  layout->StartRow(0, kSingleColumnSetId);
  layout->AddView(title_label);

  const ListValue* mobiles = service_->GetMobiles();
  if (mobiles->GetSize() == 1) {
    string16 name;
    const DictionaryValue* mobile = NULL;
    if (mobiles->GetDictionary(0, &mobile) &&
        mobile->GetString("name", &name)) {
      title_label->SetText(l10n_util::GetStringFUTF16(
          IDS_CHROME_TO_MOBILE_BUBBLE_SINGLE_TITLE, name));
    } else {
      NOTREACHED();
    }
  } else {
    title_label->SetText(l10n_util::GetStringUTF16(
        IDS_CHROME_TO_MOBILE_BUBBLE_MULTI_TITLE));

    string16 name;
    const DictionaryValue* mobile = NULL;
    views::RadioButton* radio = NULL;
    layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
    for (size_t index = 0; index < mobiles->GetSize(); ++index) {
      if (mobiles->GetDictionary(index, &mobile) &&
          mobile->GetString("name", &name)) {
        radio = new views::RadioButton(name, 0);
        radio->SetEnabledColor(SK_ColorBLACK);
        radio->SetHoverColor(SK_ColorBLACK);
        radio_buttons_.push_back(radio);
        layout->StartRow(0, kRadioColumnSetId);
        layout->AddView(radio);
      } else {
        NOTREACHED();
      }
    }
    radio_buttons_[0]->SetChecked(true);
  }

  send_copy_ = new views::Checkbox(
      l10n_util::GetStringFUTF16(IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY,
          l10n_util::GetStringUTF16(
              IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY_GENERATING)));
  send_copy_->SetEnabledColor(SK_ColorBLACK);
  send_copy_->SetHoverColor(SK_ColorBLACK);
  send_copy_->SetEnabled(false);
  layout->StartRow(0, kSingleColumnSetId);
  layout->AddView(send_copy_);

  views::Link* learn_more =
      new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more->set_listener(this);

  send_ = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_CHROME_TO_MOBILE_BUBBLE_SEND));
  send_->SetIsDefault(true);
  cancel_ = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_CANCEL));
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, kButtonColumnSetId);
  layout->AddView(learn_more);
  layout->AddView(send_);
  layout->AddView(cancel_);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
}

ChromeToMobileBubbleView::ChromeToMobileBubbleView(views::View* anchor_view,
                                                   Browser* browser)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      browser_(browser),
      service_(ChromeToMobileServiceFactory::GetForProfile(browser->profile())),
      send_copy_(NULL),
      send_(NULL),
      cancel_(NULL) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_insets(gfx::Insets(5, 0, 5, 0));

  // Generate the MHTML snapshot now to report its size in the bubble.
  service_->GenerateSnapshot(browser_, weak_ptr_factory_.GetWeakPtr());
}

void ChromeToMobileBubbleView::LinkClicked(views::Link* source,
                                           int event_flags) {
  service_->LearnMore(browser_);
  GetWidget()->Close();
}

void ChromeToMobileBubbleView::HandleButtonPressed(views::Button* sender) {
  if (sender == send_)
    Send();
  else if (sender == cancel_)
    GetWidget()->Close();
}

void ChromeToMobileBubbleView::Send() {
  // TODO(msw): Handle updates to the mobile list while the bubble is open.
  const ListValue* mobiles = service_->GetMobiles();
  size_t selected_index = 0;
  if (mobiles->GetSize() > 1) {
    DCHECK_EQ(mobiles->GetSize(), radio_buttons_.size());
    for (; selected_index < radio_buttons_.size(); ++selected_index) {
      if (radio_buttons_[selected_index]->checked())
        break;
    }
  } else {
    DCHECK(radio_buttons_.empty());
  }

  const DictionaryValue* mobile = NULL;
  if (mobiles->GetDictionary(selected_index, &mobile)) {
    base::FilePath snapshot =
        send_copy_->checked() ? snapshot_path_ : base::FilePath();
    service_->SendToMobile(mobile, snapshot, browser_,
                           weak_ptr_factory_.GetWeakPtr());
  } else {
    NOTREACHED();
  }

  // Update the view's contents to show the "Sending..." progress animation.
  cancel_->SetEnabled(false);
  send_->SetEnabled(false);
  send_->set_alignment(views::TextButtonBase::ALIGN_LEFT);
  progress_animation_.reset(new ui::ThrobAnimation(this));
  progress_animation_->SetDuration(kProgressThrobDurationMS);
  progress_animation_->StartThrobbing(-1);
}
