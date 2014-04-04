// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_bubble.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget_observer.h"

using content::WebContents;

namespace {

const int kBubbleHorizMargin = 6;
const int kBubbleVertMargin = 4;
const int kBubbleHeadingVertMargin = 6;

// This is the SpeechRecognitionBubble content and views bubble delegate.
class SpeechRecognitionBubbleView : public views::BubbleDelegateView,
                                    public views::ButtonListener,
                                    public views::LinkListener {
 public:
  SpeechRecognitionBubbleView(SpeechRecognitionBubbleDelegate* delegate,
                              views::View* anchor_view,
                              const gfx::Rect& element_rect,
                              WebContents* web_contents);

  void UpdateLayout(SpeechRecognitionBubbleBase::DisplayMode mode,
                    const base::string16& message_text,
                    const gfx::ImageSkia& image);
  void SetImage(const gfx::ImageSkia& image);

  // views::BubbleDelegateView methods.
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) OVERRIDE;
  virtual gfx::Rect GetAnchorRect() OVERRIDE;
  virtual void Init() OVERRIDE;

  // views::ButtonListener methods.
  virtual void ButtonPressed(views::Button* source,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener methods.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::View overrides.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

  void set_notify_delegate_on_activation_change(bool notify) {
    notify_delegate_on_activation_change_ = notify;
  }

 private:
  SpeechRecognitionBubbleDelegate* delegate_;
  gfx::Rect element_rect_;
  WebContents* web_contents_;
  bool notify_delegate_on_activation_change_;
  views::ImageView* icon_;
  views::Label* heading_;
  views::Label* message_;
  views::LabelButton* try_again_;
  views::LabelButton* cancel_;
  views::Link* mic_settings_;
  SpeechRecognitionBubbleBase::DisplayMode display_mode_;
  const int kIconLayoutMinWidth;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionBubbleView);
};

SpeechRecognitionBubbleView::SpeechRecognitionBubbleView(
    SpeechRecognitionBubbleDelegate* delegate,
    views::View* anchor_view,
    const gfx::Rect& element_rect,
    WebContents* web_contents)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      delegate_(delegate),
      element_rect_(element_rect),
      web_contents_(web_contents),
      notify_delegate_on_activation_change_(true),
      icon_(NULL),
      heading_(NULL),
      message_(NULL),
      try_again_(NULL),
      cancel_(NULL),
      mic_settings_(NULL),
      display_mode_(SpeechRecognitionBubbleBase::DISPLAY_MODE_WARM_UP),
      kIconLayoutMinWidth(ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                          IDR_SPEECH_INPUT_MIC_EMPTY)->width()) {
  // The bubble lifetime is managed by its controller; explicitly closing
  // on deactivation will cause unexpected behavior.
  set_close_on_deactivate(false);
  // Prevent default behavior of bubble closure on escape key and handle
  // it in the AcceleratorPressed() to avoid an unexpected behavior.
  set_close_on_esc(false);

  // Update the bubble's bounds when the window's bounds changes.
  set_move_with_anchor(true);
}

void SpeechRecognitionBubbleView::OnWidgetActivationChanged(
    views::Widget* widget, bool active) {
  if (widget == GetWidget() && !active && notify_delegate_on_activation_change_)
    delegate_->InfoBubbleFocusChanged();
  BubbleDelegateView::OnWidgetActivationChanged(widget, active);
}

gfx::Rect SpeechRecognitionBubbleView::GetAnchorRect() {
  gfx::Rect container_rect;
  web_contents_->GetView()->GetContainerBounds(&container_rect);
  gfx::Rect anchor(element_rect_);
  anchor.Offset(container_rect.OffsetFromOrigin());
  if (!container_rect.Intersects(anchor))
    return BubbleDelegateView::GetAnchorRect();
  return anchor;
}

void SpeechRecognitionBubbleView::Init() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& font_list =
      rb.GetFontList(ui::ResourceBundle::MediumFont);

  heading_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_SPEECH_INPUT_BUBBLE_HEADING), font_list);
  heading_->SetBorder(views::Border::CreateEmptyBorder(
      kBubbleHeadingVertMargin, 0, kBubbleHeadingVertMargin, 0));
  heading_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  AddChildView(heading_);

  message_ = new views::Label(base::string16(), font_list);
  message_->SetMultiLine(true);
  AddChildView(message_);

  icon_ = new views::ImageView();
  icon_->SetHorizontalAlignment(views::ImageView::CENTER);
  AddChildView(icon_);

  cancel_ = new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_->SetStyle(views::Button::STYLE_BUTTON);
  AddChildView(cancel_);

  try_again_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_SPEECH_INPUT_TRY_AGAIN));
  try_again_->SetStyle(views::Button::STYLE_BUTTON);
  AddChildView(try_again_);

  mic_settings_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_SPEECH_INPUT_MIC_SETTINGS));
  mic_settings_->set_listener(this);
  AddChildView(mic_settings_);
}

void SpeechRecognitionBubbleView::UpdateLayout(
    SpeechRecognitionBubbleBase::DisplayMode mode,
    const base::string16& message_text,
    const gfx::ImageSkia& image) {
  display_mode_ = mode;
  bool is_message = (mode == SpeechRecognitionBubbleBase::DISPLAY_MODE_MESSAGE);
  icon_->SetVisible(!is_message);
  message_->SetVisible(is_message);
  mic_settings_->SetVisible(is_message);
  try_again_->SetVisible(is_message);
  cancel_->SetVisible(
      mode != SpeechRecognitionBubbleBase::DISPLAY_MODE_WARM_UP);
  heading_->SetVisible(
      mode == SpeechRecognitionBubbleBase::DISPLAY_MODE_RECORDING);

  // Clickable elements should be enabled if and only if they are visible.
  mic_settings_->SetEnabled(mic_settings_->visible());
  try_again_->SetEnabled(try_again_->visible());
  cancel_->SetEnabled(cancel_->visible());

  if (is_message) {
    message_->SetText(message_text);
  } else {
    SetImage(image);
  }

  if (icon_->visible())
    icon_->ResetImageSize();

  // When moving from warming up to recording state, the size of the content
  // stays the same. So we wouldn't get a resize/layout call from the view
  // system and we do it ourselves.
  if (GetPreferredSize() == size())  // |size()| here is the current size.
    Layout();

  SizeToContents();
}

void SpeechRecognitionBubbleView::SetImage(const gfx::ImageSkia& image) {
  icon_->SetImage(image);
}

void SpeechRecognitionBubbleView::ButtonPressed(views::Button* source,
                                                const ui::Event& event) {
  if (source == cancel_) {
    delegate_->InfoBubbleButtonClicked(SpeechRecognitionBubble::BUTTON_CANCEL);
  } else if (source == try_again_) {
    delegate_->InfoBubbleButtonClicked(
        SpeechRecognitionBubble::BUTTON_TRY_AGAIN);
  } else {
    NOTREACHED() << "Unknown button";
  }
}

void SpeechRecognitionBubbleView::LinkClicked(views::Link* source,
                                              int event_flags) {
  DCHECK_EQ(mic_settings_, source);
  content::SpeechRecognitionManager::GetInstance()->ShowAudioInputSettings();
}

bool SpeechRecognitionBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // The accelerator is added by BubbleDelegateView.
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    delegate_->InfoBubbleButtonClicked(SpeechRecognitionBubble::BUTTON_CANCEL);
    return true;
  }

  return BubbleDelegateView::AcceleratorPressed(accelerator);
}

gfx::Size SpeechRecognitionBubbleView::GetPreferredSize() {
  int width = heading_->GetPreferredSize().width();
  int control_width = cancel_->GetPreferredSize().width();
  if (try_again_->visible()) {
    control_width += try_again_->GetPreferredSize().width() +
                     views::kRelatedButtonHSpacing;
  }
  width = std::max(width, control_width);
  control_width = std::max(icon_->GetPreferredSize().width(),
                           kIconLayoutMinWidth);
  width = std::max(width, control_width);
  if (mic_settings_->visible()) {
    control_width = mic_settings_->GetPreferredSize().width();
    width = std::max(width, control_width);
  }

  int height = cancel_->GetPreferredSize().height();
  if (message_->visible()) {
    height += message_->GetHeightForWidth(width) +
              views::kLabelToControlVerticalSpacing;
  }
  if (heading_->visible())
    height += heading_->GetPreferredSize().height();
  if (icon_->visible())
    height += icon_->GetImage().height();
  if (mic_settings_->visible())
    height += mic_settings_->GetPreferredSize().height();
  width += kBubbleHorizMargin * 2;
  height += kBubbleVertMargin * 2;

  return gfx::Size(width, height);
}

void SpeechRecognitionBubbleView::Layout() {
  int x = kBubbleHorizMargin;
  int y = kBubbleVertMargin;
  int available_width = width() - kBubbleHorizMargin * 2;
  int available_height = height() - kBubbleVertMargin * 2;

  if (message_->visible()) {
    DCHECK(try_again_->visible());

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
    DCHECK(icon_->visible());

    int control_height = icon_->GetImage().height();
    if (display_mode_ == SpeechRecognitionBubbleBase::DISPLAY_MODE_WARM_UP)
      y = (available_height - control_height) / 2;
    icon_->SetBounds(x, y, available_width, control_height);
    y += control_height;

    if (heading_->visible()) {
      control_height = heading_->GetPreferredSize().height();
      heading_->SetBounds(x, y, available_width, control_height);
      y += control_height;
    }

    if (cancel_->visible()) {
      control_height = cancel_->GetPreferredSize().height();
      int width = cancel_->GetPreferredSize().width();
      cancel_->SetBounds(x + (available_width - width) / 2, y, width,
                         control_height);
    }
  }
}

// Implementation of SpeechRecognitionBubble.
class SpeechRecognitionBubbleImpl
    : public SpeechRecognitionBubbleBase,
      public views::WidgetObserver {
 public:
  SpeechRecognitionBubbleImpl(int render_process_id, int render_view_id,
                              Delegate* delegate,
                              const gfx::Rect& element_rect);
  virtual ~SpeechRecognitionBubbleImpl();

  // SpeechRecognitionBubble methods.
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;

  // SpeechRecognitionBubbleBase methods.
  virtual void UpdateLayout() OVERRIDE;
  virtual void UpdateImage() OVERRIDE;

  // views::WidgetObserver methods.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

 private:
  Delegate* delegate_;
  SpeechRecognitionBubbleView* bubble_;
  gfx::Rect element_rect_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionBubbleImpl);
};

SpeechRecognitionBubbleImpl::SpeechRecognitionBubbleImpl(
    int render_process_id, int render_view_id, Delegate* delegate,
    const gfx::Rect& element_rect)
    : SpeechRecognitionBubbleBase(render_process_id, render_view_id),
      delegate_(delegate),
      bubble_(NULL),
      element_rect_(element_rect) {
}

SpeechRecognitionBubbleImpl::~SpeechRecognitionBubbleImpl() {
  if (bubble_) {
    bubble_->GetWidget()->RemoveObserver(this);
    bubble_->set_notify_delegate_on_activation_change(false);
    bubble_->GetWidget()->Close();
  }
}

void SpeechRecognitionBubbleImpl::OnWidgetDestroying(views::Widget* widget) {
  bubble_ = NULL;
}

void SpeechRecognitionBubbleImpl::Show() {
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;

  if (!bubble_) {
    views::View* icon = NULL;

    // Anchor to the location bar, in case |element_rect| is offscreen.
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    if (browser) {
      BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(browser);
      icon = browser_view->GetLocationBarView() ?
          browser_view->GetLocationBarView()->GetLocationBarAnchor() : NULL;
    }

    bubble_ = new SpeechRecognitionBubbleView(delegate_, icon, element_rect_,
                                              web_contents);

    if (!icon) {
      // We dont't have an icon to attach to. Manually specify the web contents
      // window as the parent.
      bubble_->set_parent_window(
          web_contents->GetView()->GetTopLevelNativeWindow());
    }

    views::BubbleDelegateView::CreateBubble(bubble_);
    UpdateLayout();
    bubble_->GetWidget()->AddObserver(this);
   }
  bubble_->GetWidget()->Show();
}

void SpeechRecognitionBubbleImpl::Hide() {
  if (bubble_)
    bubble_->GetWidget()->Hide();
}

void SpeechRecognitionBubbleImpl::UpdateLayout() {
  if (bubble_ && GetWebContents())
    bubble_->UpdateLayout(display_mode(), message_text(), icon_image());
}

void SpeechRecognitionBubbleImpl::UpdateImage() {
  if (bubble_ && GetWebContents())
    bubble_->SetImage(icon_image());
}

}  // namespace

SpeechRecognitionBubble* SpeechRecognitionBubble::CreateNativeBubble(
    int render_process_id,
    int render_view_id,
    SpeechRecognitionBubble::Delegate* delegate,
    const gfx::Rect& element_rect) {
  return new SpeechRecognitionBubbleImpl(render_process_id, render_view_id,
      delegate, element_rect);
}
