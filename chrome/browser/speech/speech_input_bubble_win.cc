// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_bubble.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "gfx/canvas.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/standard_layout.h"
#include "views/view.h"

namespace {

// The speech bubble's arrow is so many pixels from the left of html element.
const int kBubbleTargetOffsetX = 5;
const int kBubbleHorizMargin = 40;
const int kBubbleVertMargin = 0;

// This is the content view which is placed inside a SpeechInputBubble.
class ContentView
    : public views::View,
      public views::LinkController {
 public:
  explicit ContentView(SpeechInputBubbleDelegate* delegate);

  void SetRecognizingMode();

  // views::LinkController methods.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // views::View overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

 private:
  SpeechInputBubbleDelegate* delegate_;
  views::ImageView* icon_;
  views::Label* heading_;
  views::Link* cancel_;

  DISALLOW_COPY_AND_ASSIGN(ContentView);
};

ContentView::ContentView(SpeechInputBubbleDelegate* delegate)
     : delegate_(delegate) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font = rb.GetFont(ResourceBundle::BaseFont);

  heading_ = new views::Label(
      l10n_util::GetString(IDS_SPEECH_INPUT_BUBBLE_HEADING));
  heading_->SetFont(font.DeriveFont(3, gfx::Font::NORMAL));
  heading_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  AddChildView(heading_);

  icon_ = new views::ImageView();
  icon_->SetImage(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_SPEECH_INPUT_RECORDING));
  icon_->SetHorizontalAlignment(views::ImageView::CENTER);
  AddChildView(icon_);

  cancel_ = new views::Link(l10n_util::GetString(IDS_CANCEL));
  cancel_->SetController(this);
  cancel_->SetFont(font.DeriveFont(3, gfx::Font::NORMAL));
  cancel_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  AddChildView(cancel_);
}

void ContentView::SetRecognizingMode() {
  icon_->SetImage(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_SPEECH_INPUT_PROCESSING));
}

void ContentView::LinkActivated(views::Link* source, int event_flags) {
  if (source == cancel_) {
    delegate_->RecognitionCancelled();
  } else {
    NOTREACHED() << "Unknown view";
  }
}

gfx::Size ContentView::GetPreferredSize() {
  int width = heading_->GetPreferredSize().width();
  int control_width = cancel_->GetPreferredSize().width();
  if (control_width > width)
    width = control_width;
  control_width = icon_->GetPreferredSize().width();
  if (control_width > width)
    width = control_width;
  width += kBubbleHorizMargin * 2;

  int height = kBubbleVertMargin * 2 +
               heading_->GetPreferredSize().height() +
               cancel_->GetPreferredSize().height() +
               icon_->GetImage().height();
  return gfx::Size(width, height);
}

void ContentView::Layout() {
  int x = kBubbleHorizMargin;
  int y = kBubbleVertMargin;
  int control_width = width() - kBubbleHorizMargin * 2;

  int height = heading_->GetPreferredSize().height();
  heading_->SetBounds(x, y, control_width, height);
  y += height;

  height = icon_->GetImage().height();
  icon_->SetBounds(x, y, control_width, height);
  y += height;

  height = cancel_->GetPreferredSize().height();
  cancel_->SetBounds(x, y, control_width, height);
}

// Implementation of SpeechInputBubble.
class SpeechInputBubbleImpl
    : public SpeechInputBubble,
      public InfoBubbleDelegate,
      public NotificationObserver {
 public:
  SpeechInputBubbleImpl(TabContents* tab_contents,
                        Delegate* delegate,
                        const gfx::Rect& element_rect);
  virtual ~SpeechInputBubbleImpl();

  virtual void SetRecognizingMode();

  // Returns the screen rectangle to use as the info bubble's target.
  // |element_rect| is the html element's bounds in page coordinates.
  gfx::Rect GetInfoBubbleTarget(const gfx::Rect& element_rect);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // InfoBubbleDelegate
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape();
  virtual bool FadeInOnShow();

 private:
  Delegate* delegate_;
  InfoBubble* info_bubble_;
  TabContents* tab_contents_;
  ContentView* bubble_content_;
  NotificationRegistrar registrar_;

  // Set to true if the object is being destroyed normally instead of the
  // user clicking outside the window causing it to close automatically.
  bool did_invoke_close_;

  DISALLOW_COPY_AND_ASSIGN(SpeechInputBubbleImpl);
};

SpeechInputBubbleImpl::SpeechInputBubbleImpl(TabContents* tab_contents,
                                             Delegate* delegate,
                                             const gfx::Rect& element_rect)
    : delegate_(delegate),
      info_bubble_(NULL),
      tab_contents_(tab_contents),
      bubble_content_(NULL),
      did_invoke_close_(false) {
  bubble_content_ = new ContentView(delegate_);

  views::Widget* parent = views::Widget::GetWidgetFromNativeWindow(
      tab_contents_->view()->GetTopLevelNativeWindow());
  info_bubble_ = InfoBubble::Show(parent,
                                  GetInfoBubbleTarget(element_rect),
                                  BubbleBorder::TOP_LEFT, bubble_content_,
                                  this);

  // We don't want fade outs when closing because it makes speech recognition
  // appear slower than it is. Also setting it to false allows |Close| to
  // destroy the bubble immediately instead of waiting for the fade animation
  // to end so the caller can manage this object's life cycle like a normal
  // stack based or member variable object.
  info_bubble_->set_fade_away_on_close(false);

  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab_contents_));
}

SpeechInputBubbleImpl::~SpeechInputBubbleImpl() {
  if (info_bubble_) {
    did_invoke_close_ = true;
    info_bubble_->Close();
  }
}

void SpeechInputBubbleImpl::SetRecognizingMode() {
  DCHECK(info_bubble_);
  DCHECK(bubble_content_);
  bubble_content_->SetRecognizingMode();
}

gfx::Rect SpeechInputBubbleImpl::GetInfoBubbleTarget(
    const gfx::Rect& element_rect) {
  gfx::Rect container_rect;
  tab_contents_->GetContainerBounds(&container_rect);
  return gfx::Rect(
      container_rect.x() + element_rect.x() + kBubbleTargetOffsetX,
      container_rect.y() + element_rect.y() + element_rect.height(), 1, 1);
}

void SpeechInputBubbleImpl::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (type == NotificationType::TAB_CONTENTS_DESTROYED) {
    delegate_->RecognitionCancelled();
  } else {
    NOTREACHED() << "Unknown notification";
  }
}

void SpeechInputBubbleImpl::InfoBubbleClosing(InfoBubble* info_bubble,
                                          bool closed_by_escape) {
  registrar_.Remove(this, NotificationType::TAB_CONTENTS_DESTROYED,
                    Source<TabContents>(tab_contents_));
  info_bubble_ = NULL;
  bubble_content_ = NULL;
  if (!did_invoke_close_)
    delegate_->InfoBubbleClosed();
}

bool SpeechInputBubbleImpl::CloseOnEscape() {
  return false;
}

bool SpeechInputBubbleImpl::FadeInOnShow() {
  return false;
}

}  // namespace

SpeechInputBubble* SpeechInputBubble::CreateNativeBubble(
    TabContents* tab_contents,
    SpeechInputBubble::Delegate* delegate,
    const gfx::Rect& element_rect) {
  return new SpeechInputBubbleImpl(tab_contents, delegate, element_rect);
}
