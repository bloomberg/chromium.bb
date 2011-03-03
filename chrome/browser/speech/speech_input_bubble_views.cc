// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_bubble.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/ui/views/info_bubble.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "media/audio/audio_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/border.h"
#include "views/controls/button/native_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/layout/layout_constants.h"
#include "views/view.h"

namespace {

const int kBubbleHorizMargin = 6;
const int kBubbleVertMargin = 4;
const int kBubbleHeadingVertMargin = 6;

// This is the content view which is placed inside a SpeechInputBubble.
class ContentView
    : public views::View,
      public views::ButtonListener,
      public views::LinkController {
 public:
  explicit ContentView(SpeechInputBubbleDelegate* delegate);

  void UpdateLayout(SpeechInputBubbleBase::DisplayMode mode,
                    const string16& message_text);
  void SetImage(const SkBitmap& image);

  // views::ButtonListener methods.
  virtual void ButtonPressed(views::Button* source, const views::Event& event);

  // views::LinkController methods.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // views::View overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

 private:
  SpeechInputBubbleDelegate* delegate_;
  views::ImageView* icon_;
  views::Label* heading_;
  views::Label* message_;
  views::NativeButton* try_again_;
  views::NativeButton* cancel_;
  views::Link* mic_settings_;

  DISALLOW_COPY_AND_ASSIGN(ContentView);
};

ContentView::ContentView(SpeechInputBubbleDelegate* delegate)
     : delegate_(delegate) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font = rb.GetFont(ResourceBundle::MediumFont);

  heading_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_SPEECH_INPUT_BUBBLE_HEADING)));
  heading_->set_border(views::Border::CreateEmptyBorder(
      kBubbleHeadingVertMargin, 0, kBubbleHeadingVertMargin, 0));
  heading_->SetFont(font);
  heading_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  heading_->SetText(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_SPEECH_INPUT_BUBBLE_HEADING)));
  AddChildView(heading_);

  message_ = new views::Label();
  message_->SetFont(font);
  message_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  message_->SetMultiLine(true);
  AddChildView(message_);

  icon_ = new views::ImageView();
  icon_->SetImage(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_SPEECH_INPUT_MIC_EMPTY));
  icon_->SetHorizontalAlignment(views::ImageView::CENTER);
  AddChildView(icon_);

  cancel_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CANCEL)));
  AddChildView(cancel_);

  try_again_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_SPEECH_INPUT_TRY_AGAIN)));
  AddChildView(try_again_);

  mic_settings_ = new views::Link(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_SPEECH_INPUT_MIC_SETTINGS)));
  mic_settings_->SetController(this);
  AddChildView(mic_settings_);
}

void ContentView::UpdateLayout(SpeechInputBubbleBase::DisplayMode mode,
                               const string16& message_text) {
  bool is_message = (mode == SpeechInputBubbleBase::DISPLAY_MODE_MESSAGE);
  icon_->SetVisible(!is_message);
  message_->SetVisible(is_message);
  mic_settings_->SetVisible(is_message);
  try_again_->SetVisible(is_message);
  heading_->SetVisible(mode == SpeechInputBubbleBase::DISPLAY_MODE_RECORDING);

  if (mode == SpeechInputBubbleBase::DISPLAY_MODE_MESSAGE) {
    message_->SetText(UTF16ToWideHack(message_text));
  } else if (mode == SpeechInputBubbleBase::DISPLAY_MODE_RECORDING) {
    icon_->SetImage(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_SPEECH_INPUT_MIC_EMPTY));
  }

  if (icon_->IsVisible())
    icon_->ResetImageSize();
}

void ContentView::SetImage(const SkBitmap& image) {
  icon_->SetImage(image);
}

void ContentView::ButtonPressed(views::Button* source,
                                const views::Event& event) {
  if (source == cancel_) {
    delegate_->InfoBubbleButtonClicked(SpeechInputBubble::BUTTON_CANCEL);
  } else if (source == try_again_) {
    delegate_->InfoBubbleButtonClicked(SpeechInputBubble::BUTTON_TRY_AGAIN);
  } else {
    NOTREACHED() << "Unknown button";
  }
}

void ContentView::LinkActivated(views::Link* source, int event_flags) {
  DCHECK_EQ(source, mic_settings_);
  AudioManager::GetAudioManager()->ShowAudioInputSettings();
}

gfx::Size ContentView::GetPreferredSize() {
  int width = heading_->GetPreferredSize().width();
  int control_width = cancel_->GetPreferredSize().width();
  if (try_again_->IsVisible()) {
    control_width += try_again_->GetPreferredSize().width() +
                     views::kRelatedButtonHSpacing;
  }
  if (control_width > width)
    width = control_width;
  control_width = icon_->GetPreferredSize().width();
  if (control_width > width)
    width = control_width;
  if (mic_settings_->IsVisible()) {
    control_width = mic_settings_->GetPreferredSize().width();
    if (control_width > width)
      width = control_width;
  }

  int height = cancel_->GetPreferredSize().height();
  if (message_->IsVisible()) {
    height += message_->GetHeightForWidth(width) +
              views::kLabelToControlVerticalSpacing;
  }
  if (heading_->IsVisible())
    height += heading_->GetPreferredSize().height();
  if (icon_->IsVisible())
    height += icon_->GetImage().height();
  if (mic_settings_->IsVisible())
    height += mic_settings_->GetPreferredSize().height();
  width += kBubbleHorizMargin * 2;
  height += kBubbleVertMargin * 2;

  return gfx::Size(width, height);
}

void ContentView::Layout() {
  int x = kBubbleHorizMargin;
  int y = kBubbleVertMargin;
  int available_width = width() - kBubbleHorizMargin * 2;
  int available_height = height() - kBubbleVertMargin * 2;

  if (message_->IsVisible()) {
    DCHECK(try_again_->IsVisible());

    int control_height = try_again_->GetPreferredSize().height();
    int try_again_width = try_again_->GetPreferredSize().width();
    int cancel_width = cancel_->GetPreferredSize().width();
    y += available_height - control_height;
    x += (available_width - cancel_width - try_again_width -
          views::kRelatedButtonHSpacing) / 2;
    try_again_->SetBounds(x, y, try_again_width, control_height);
    cancel_->SetBounds(x + try_again_width + views::kRelatedButtonHSpacing, y,
                       cancel_width, control_height);

    control_height = message_->GetHeightForWidth(available_width);
    message_->SetBounds(kBubbleHorizMargin, kBubbleVertMargin,
                        available_width, control_height);
    y = kBubbleVertMargin + control_height;

    control_height = mic_settings_->GetPreferredSize().height();
    mic_settings_->SetBounds(kBubbleHorizMargin, y, available_width,
                             control_height);
  } else {
    DCHECK(icon_->IsVisible());

    int control_height = icon_->GetImage().height();
    icon_->SetBounds(x, y, available_width, control_height);
    y += control_height;

    if (heading_->IsVisible()) {
      control_height = heading_->GetPreferredSize().height();
      heading_->SetBounds(x, y, available_width, control_height);
      y += control_height;
    }

    control_height = cancel_->GetPreferredSize().height();
    int width = cancel_->GetPreferredSize().width();
    cancel_->SetBounds(x + (available_width - width) / 2, y, width,
                       control_height);
  }
}

// Implementation of SpeechInputBubble.
class SpeechInputBubbleImpl
    : public SpeechInputBubbleBase,
      public InfoBubbleDelegate {
 public:
  SpeechInputBubbleImpl(TabContents* tab_contents,
                        Delegate* delegate,
                        const gfx::Rect& element_rect);
  virtual ~SpeechInputBubbleImpl();

  // SpeechInputBubble methods.
  virtual void Show();
  virtual void Hide();

  // SpeechInputBubbleBase methods.
  virtual void UpdateLayout();
  virtual void SetImage(const SkBitmap& image);

  // Returns the screen rectangle to use as the info bubble's target.
  // |element_rect| is the html element's bounds in page coordinates.
  gfx::Rect GetInfoBubbleTarget(const gfx::Rect& element_rect);

  // InfoBubbleDelegate
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape();
  virtual bool FadeInOnShow();

 private:
  Delegate* delegate_;
  InfoBubble* info_bubble_;
  ContentView* bubble_content_;
  gfx::Rect element_rect_;

  // Set to true if the object is being destroyed normally instead of the
  // user clicking outside the window causing it to close automatically.
  bool did_invoke_close_;

  DISALLOW_COPY_AND_ASSIGN(SpeechInputBubbleImpl);
};

SpeechInputBubbleImpl::SpeechInputBubbleImpl(TabContents* tab_contents,
                                             Delegate* delegate,
                                             const gfx::Rect& element_rect)
    : SpeechInputBubbleBase(tab_contents),
      delegate_(delegate),
      info_bubble_(NULL),
      bubble_content_(NULL),
      element_rect_(element_rect),
      did_invoke_close_(false) {
}

SpeechInputBubbleImpl::~SpeechInputBubbleImpl() {
  did_invoke_close_ = true;
  Hide();
}

gfx::Rect SpeechInputBubbleImpl::GetInfoBubbleTarget(
    const gfx::Rect& element_rect) {
  gfx::Rect container_rect;
  tab_contents()->GetContainerBounds(&container_rect);
  return gfx::Rect(
      container_rect.x() + element_rect.x() + element_rect.width() -
          kBubbleTargetOffsetX,
      container_rect.y() + element_rect.y() + element_rect.height(), 1, 1);
}

void SpeechInputBubbleImpl::InfoBubbleClosing(InfoBubble* info_bubble,
                                              bool closed_by_escape) {
  info_bubble_ = NULL;
  bubble_content_ = NULL;
  if (!did_invoke_close_)
    delegate_->InfoBubbleFocusChanged();
}

bool SpeechInputBubbleImpl::CloseOnEscape() {
  return false;
}

bool SpeechInputBubbleImpl::FadeInOnShow() {
  return false;
}

void SpeechInputBubbleImpl::Show() {
  if (info_bubble_)
    return;  // nothing to do, already visible.

  bubble_content_ = new ContentView(delegate_);
  UpdateLayout();

  views::NativeWidget* toplevel_widget =
      views::NativeWidget::GetTopLevelNativeWidget(
          tab_contents()->view()->GetNativeView());
  if (toplevel_widget) {
    info_bubble_ = InfoBubble::Show(toplevel_widget->GetWidget(),
                                    GetInfoBubbleTarget(element_rect_),
                                    BubbleBorder::TOP_LEFT, bubble_content_,
                                    this);

    // We don't want fade outs when closing because it makes speech recognition
    // appear slower than it is. Also setting it to false allows |Close| to
    // destroy the bubble immediately instead of waiting for the fade animation
    // to end so the caller can manage this object's life cycle like a normal
    // stack based or member variable object.
    info_bubble_->set_fade_away_on_close(false);
  }
}

void SpeechInputBubbleImpl::Hide() {
  if (info_bubble_)
    info_bubble_->Close();
}

void SpeechInputBubbleImpl::UpdateLayout() {
  if (bubble_content_)
    bubble_content_->UpdateLayout(display_mode(), message_text());
  if (info_bubble_)  // Will be null on first call.
    info_bubble_->SizeToContents();
}

void SpeechInputBubbleImpl::SetImage(const SkBitmap& image) {
  if (bubble_content_)
    bubble_content_->SetImage(image);
}

}  // namespace

SpeechInputBubble* SpeechInputBubble::CreateNativeBubble(
    TabContents* tab_contents,
    SpeechInputBubble::Delegate* delegate,
    const gfx::Rect& element_rect) {
  return new SpeechInputBubbleImpl(tab_contents, delegate, element_rect);
}
