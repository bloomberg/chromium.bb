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
#include "chrome/browser/profiles/profile.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/events/event.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

using views::GridLayout;

namespace {

// The title label's color; matches the bookmark bubble's title.
const SkColor kTitleColor = 0xFF062D75;

// The millisecond duration of the "Sending..." progress throb animation.
const size_t kProgressThrobDurationMS = 2400;

// The seconds to delay before automatically closing the bubble after sending.
const int kAutoCloseDelay = 3;

// A custom TextButtonNativeThemeBorder with no left (leading) inset.
class CheckboxNativeThemeBorder : public views::TextButtonNativeThemeBorder {
 public:
  explicit CheckboxNativeThemeBorder(views::NativeThemeDelegate* delegate);
  virtual ~CheckboxNativeThemeBorder();

  // views::TextButtonNativeThemeBorder methods.
  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CheckboxNativeThemeBorder);
};

CheckboxNativeThemeBorder::CheckboxNativeThemeBorder(
    views::NativeThemeDelegate* delegate)
    : views::TextButtonNativeThemeBorder(delegate) {}

CheckboxNativeThemeBorder::~CheckboxNativeThemeBorder() {}

void CheckboxNativeThemeBorder::GetInsets(gfx::Insets* insets) const {
  views::TextButtonNativeThemeBorder::GetInsets(insets);
  insets->Set(insets->top(), 0, insets->bottom(), insets->right());
}

// Downcast the View to an ImageView and set the image with the resource id.
void SetImageViewToId(views::View* image_view, int id) {
  views::ImageView* image = static_cast<views::ImageView*>(image_view);
  if (image)
    image->SetImage(ui::ResourceBundle::GetSharedInstance().GetBitmapNamed(id));
}

}  // namespace

// Declared in browser_dialogs.h so callers don't have to depend on our header.

namespace browser {

void ShowChromeToMobileBubbleView(views::View* anchor_view, Profile* profile) {
  ChromeToMobileBubbleView::ShowBubble(anchor_view, profile);
}

void HideChromeToMobileBubbleView() {
  ChromeToMobileBubbleView::Hide();
}

bool IsChromeToMobileBubbleViewShowing() {
  return ChromeToMobileBubbleView::IsShowing();
}

}  // namespace browser

// ChromeToMobileBubbleView ----------------------------------------------------

ChromeToMobileBubbleView* ChromeToMobileBubbleView::bubble_ = NULL;

ChromeToMobileBubbleView::~ChromeToMobileBubbleView() {}

// static
void ChromeToMobileBubbleView::ShowBubble(views::View* anchor_view,
                                          Profile* profile) {
  if (IsShowing())
    return;

  // Show the lit mobile device icon during the bubble's lifetime.
  SetImageViewToId(anchor_view, IDR_MOBILE_LIT);
  bubble_ = new ChromeToMobileBubbleView(anchor_view, profile);
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

gfx::Rect ChromeToMobileBubbleView::GetAnchorRect() {
  // Compensate for some built-in padding in the arrow image.
  gfx::Rect rect(BubbleDelegateView::GetAnchorRect());
  rect.Inset(0, anchor_view() ? 5 : 0);
  return rect;
}

void ChromeToMobileBubbleView::WindowClosing() {
  // We have to reset |bubble_| here, not in our destructor, because we'll be
  // destroyed asynchronously and the shown state will be checked before then.
  DCHECK(bubble_ == this);
  bubble_ = NULL;

  // Instruct the service to delete the snapshot file.
  service_->DeleteSnapshot(snapshot_path_);

  // Restore the resting state mobile device icon.
  SetImageViewToId(anchor_view(), IDR_MOBILE);
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
                                             const views::Event& event) {
  HandleButtonPressed(sender);
}

void ChromeToMobileBubbleView::SnapshotGenerated(const FilePath& path,
                                                 int64 bytes) {
  snapshot_path_ = path;
  if (bytes > 0) {
    service_->LogMetric(ChromeToMobileService::SNAPSHOT_GENERATED);
    send_copy_->SetText(l10n_util::GetStringFUTF16(
        IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY, ui::FormatBytes(bytes)));
    send_copy_->SetEnabled(true);
  } else {
    service_->LogMetric(ChromeToMobileService::SNAPSHOT_ERROR);
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
    layout->StartRow(0, 0 /*single_column_set_id*/);
    layout->AddView(error_label);
    SizeToContents();
  }

  Layout();
}

void ChromeToMobileBubbleView::Init() {
  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  const size_t single_column_set_id = 0;
  views::ColumnSet* cs = layout->AddColumnSet(single_column_set_id);
  cs->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);

  const size_t button_column_set_id = 1;
  cs = layout->AddColumnSet(button_column_set_id);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);
  // Subtract 2px for the natural button padding and to correspond with row
  // separation height; like BookmarkBubbleView.
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing - 2);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);

  std::vector<DictionaryValue*> mobiles = service_->mobiles();
  DCHECK_GT(mobiles.size(), 0U);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  views::Label* title_label = new views::Label();
  title_label->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  title_label->SetEnabledColor(kTitleColor);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(title_label);

  if (mobiles.size() == 1) {
    selected_mobile_ = mobiles[0];
    string16 mobile_name;
    mobiles[0]->GetString("name", &mobile_name);
    title_label->SetText(l10n_util::GetStringFUTF16(
        IDS_CHROME_TO_MOBILE_BUBBLE_SINGLE_TITLE, mobile_name));
  } else {
    title_label->SetText(l10n_util::GetStringUTF16(
        IDS_CHROME_TO_MOBILE_BUBBLE_MULTI_TITLE));

    views::RadioButton* radio = NULL;
    layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
    for (std::vector<DictionaryValue*>::const_iterator it = mobiles.begin();
         it != mobiles.end(); ++it) {
      string16 name;
      (*it)->GetString("name", &name);
      radio = new views::RadioButton(name, 0);
      radio->set_listener(this);
      radio->SetEnabledColor(SK_ColorBLACK);
      radio->SetHoverColor(SK_ColorBLACK);
      mobile_map_[radio] = *it;
      layout->StartRow(0, single_column_set_id);
      layout->AddView(radio);
    }
    mobile_map_.begin()->first->SetChecked(true);
    selected_mobile_ = mobile_map_.begin()->second;
  }

  send_copy_ = new views::Checkbox(
      l10n_util::GetStringFUTF16(IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY,
          l10n_util::GetStringUTF16(
              IDS_CHROME_TO_MOBILE_BUBBLE_SEND_COPY_GENERATING)));
  // Use CheckboxNativeThemeBorder to align the checkbox with the title label.
  send_copy_->set_border(new CheckboxNativeThemeBorder(send_copy_));
  send_copy_->SetEnabledColor(SK_ColorBLACK);
  send_copy_->SetHoverColor(SK_ColorBLACK);
  send_copy_->SetEnabled(false);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(send_copy_);

  send_ = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_CHROME_TO_MOBILE_BUBBLE_SEND));
  send_->SetIsDefault(true);
  cancel_ = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_CANCEL));
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, button_column_set_id);
  layout->AddView(send_);
  layout->AddView(cancel_);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
}

ChromeToMobileBubbleView::ChromeToMobileBubbleView(views::View* anchor_view,
                                                   Profile* profile)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      service_(ChromeToMobileServiceFactory::GetForProfile(profile)),
      selected_mobile_(NULL),
      send_copy_(NULL),
      send_(NULL),
      cancel_(NULL) {
  service_->LogMetric(ChromeToMobileService::BUBBLE_SHOWN);

  // Generate the MHTML snapshot now to report its size in the bubble.
  service_->GenerateSnapshot(weak_ptr_factory_.GetWeakPtr());

  // Request a mobile device list update.
  service_->RequestMobileListUpdate();
}

void ChromeToMobileBubbleView::HandleButtonPressed(views::Button* sender) {
  if (sender == send_) {
    Send();
  } else if (sender == cancel_) {
    GetWidget()->Close();
  } else {
    // The sender is a mobile radio button
    views::RadioButton* radio = static_cast<views::RadioButton*>(sender);
    DCHECK(mobile_map_.find(radio) != mobile_map_.end());
    selected_mobile_ = mobile_map_.find(radio)->second;
  }
}

void ChromeToMobileBubbleView::Send() {
  string16 mobile_id;
  selected_mobile_->GetString("id", &mobile_id);
  FilePath snapshot = send_copy_->checked() ? snapshot_path_ : FilePath();
  service_->SendToMobile(mobile_id, snapshot, weak_ptr_factory_.GetWeakPtr());

  // Update the view's contents to show the "Sending..." progress animation.
  cancel_->SetEnabled(false);
  send_->SetEnabled(false);
  send_->set_alignment(views::TextButtonBase::ALIGN_LEFT);
  progress_animation_.reset(new ui::ThrobAnimation(this));
  progress_animation_->SetDuration(kProgressThrobDurationMS);
  progress_animation_->StartThrobbing(-1);
}
