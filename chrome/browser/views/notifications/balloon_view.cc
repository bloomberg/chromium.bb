// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/notifications/balloon_view.h"

#include <vector>

#include "app/gfx/canvas.h"
#include "app/gfx/gdi_util.h"
#include "app/gfx/insets.h"
#include "app/gfx/native_widget_types.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/notifications/balloon_view_host.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/controls/native/native_view_host.h"
#include "views/painter.h"
#include "views/widget/widget_win.h"

using views::Widget;

namespace {

// How many pixels of overlap there is between the shelf top and the
// balloon bottom.
const int kTopMargin = 1;
const int kBottomMargin = 1;
const int kLeftMargin = 1;
const int kRightMargin = 1;
const int kShelfBorderTopOverlap = 2;

// Properties of the dismiss button.
const int kDismissButtonWidth = 46;
const int kDismissButtonHeight = 20;

// Properties of the options menu.
const int kOptionsMenuWidth = 48;
const int kOptionsMenuHeight = 20;

// Properties of the origin label.
const int kLeftLabelMargin = 5;

// TODO(johnnyg): Add a shadow for the frame.
const int kLeftShadowWidth = 0;
const int kRightShadowWidth = 0;
const int kTopShadowWidth = 0;
const int kBottomShadowWidth = 0;

// Optional animation.
const bool kAnimateEnabled = true;

// The shelf height for the system default font size.  It is scaled
// with changes in the default font size.
const int kDefaultShelfHeight = 22;

// Menu commands
const int kRevokePermissionCommand = 0;

}  // namespace

class BalloonCloseButtonListener : public views::ButtonListener {
 public:
  explicit BalloonCloseButtonListener(BalloonView* view)
      : view_(view) {}
  virtual ~BalloonCloseButtonListener() {}

  // The only button currently is the close button.
  virtual void ButtonPressed(views::Button* sender, const views::Event&) {
    view_->Close(true);
  }

 private:
  // Non-owned pointer to the view which owns this object.
  BalloonView* view_;
};

BalloonViewImpl::BalloonViewImpl()
    : balloon_(NULL),
      frame_container_(NULL),
      html_container_(NULL),
      html_contents_(NULL),
      shelf_background_(NULL),
      balloon_background_(NULL),
      close_button_(NULL),
      close_button_listener_(NULL),
      animation_(NULL),
      options_menu_contents_(NULL),
      options_menu_menu_(NULL),
      options_menu_button_(NULL),
      method_factory_(this) {
  // This object is not to be deleted by the views hierarchy,
  // as it is owned by the balloon.
  set_parent_owned(false);

  // Load the sprites for the frames.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap* shelf_bitmap = rb.GetBitmapNamed(IDR_BALLOON_SHELF);
  SkBitmap* border_bitmap = rb.GetBitmapNamed(IDR_BALLOON_BORDER);

  // Insets are such because the sprites have 3x3 corners.
  gfx::Insets insets(3, 3, 3, 3);
  shelf_background_.reset(
      views::Painter::CreateImagePainter(*shelf_bitmap, insets, true));
  balloon_background_.reset(
      views::Painter::CreateImagePainter(*border_bitmap, insets, false));
}

BalloonViewImpl::~BalloonViewImpl() {
}

void BalloonViewImpl::Close(bool by_user) {
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &BalloonViewImpl::DelayedClose, by_user));
}

void BalloonViewImpl::RunMenu(views::View* source, const gfx::Point& pt) {
  RunOptionsMenu(pt);
}

void BalloonViewImpl::DelayedClose(bool by_user) {
  html_contents_->Shutdown();
  html_container_->CloseNow();
  frame_container_->CloseNow();
  balloon_->OnClose(by_user);
}

void BalloonViewImpl::DidChangeBounds(const gfx::Rect& previous,
                                      const gfx::Rect& current) {
  SizeContentsWindow();
}

void BalloonViewImpl::SizeContentsWindow() {
  if (!html_container_ || !frame_container_)
    return;

  gfx::Rect contents_rect = GetContentsRectangle();
  html_container_->SetBounds(contents_rect);
  html_container_->MoveAbove(frame_container_);

  gfx::Path path;
  GetContentsMask(contents_rect, &path);
  html_container_->SetShape(path.CreateNativeRegion());
}

void BalloonViewImpl::RepositionToBalloon() {
  DCHECK(frame_container_);
  DCHECK(html_container_);
  DCHECK(balloon_);

  if (!kAnimateEnabled) {
    frame_container_->SetBounds(
        gfx::Rect(balloon_->position(), balloon_->size()));
    gfx::Rect contents_rect = GetContentsRectangle();
    html_container_->SetBounds(contents_rect);
    return;
  }

  anim_frame_end_ = gfx::Rect(balloon_->position().x(),
                              balloon_->position().y(),
                              balloon_->size().width(),
                              balloon_->size().height());
  frame_container_->GetBounds(&anim_frame_start_, false);
  animation_.reset(new SlideAnimation(this));
  animation_->Show();
}

void BalloonViewImpl::AnimationProgressed(const Animation* animation) {
  DCHECK(animation == animation_.get());

  // Linear interpolation from start to end position.
  double e = animation->GetCurrentValue();
  double s = (1.0 - e);

  gfx::Rect frame_position(
    static_cast<int>(s * anim_frame_start_.x() +
                     e * anim_frame_end_.x()),
    static_cast<int>(s * anim_frame_start_.y() +
                     e * anim_frame_end_.y()),
    static_cast<int>(s * anim_frame_start_.width() +
                     e * anim_frame_end_.width()),
    static_cast<int>(s * anim_frame_start_.height() +
                     e * anim_frame_end_.height()));

  frame_container_->SetBounds(frame_position);
  html_container_->SetBounds(GetContentsRectangle());
}

void BalloonViewImpl::Show(Balloon* balloon) {
  balloon_ = balloon;
  close_button_listener_.reset(new BalloonCloseButtonListener(this));

  SetBounds(balloon_->position().x(), balloon_->position().y(),
            balloon_->size().width(), balloon_->size().height());

  // We have to create two windows: one for the contents and one for the
  // frame.  Why?
  // * The contents is an html window which cannot be a
  //   layered window (because it may have child windows for instance).
  // * The frame is a layered window so that we can have nicely rounded
  //   corners using alpha blending (and we may do other alpha blending
  //   effects).
  // Unfortunately, layered windows cannot have child windows. (Well, they can
  // but the child windows don't render).
  //
  // We carefully keep these two windows in sync to present the illusion of
  // one window to the user.
  gfx::Rect contents_rect = GetContentsRectangle();
  html_contents_ = new BalloonViewHost(balloon);
  html_contents_->SetPreferredSize(gfx::Size(10000, 10000));

  html_container_ = Widget::CreatePopupWidget(Widget::NotTransparent,
                                              Widget::AcceptEvents,
                                              Widget::DeleteOnDestroy);
  html_container_->SetAlwaysOnTop(true);
  html_container_->Init(NULL, contents_rect);
  html_container_->SetContentsView(html_contents_);

  gfx::Rect balloon_rect(x(), y(), width(), height());
  frame_container_ = Widget::CreatePopupWidget(Widget::Transparent,
                                               Widget::AcceptEvents,
                                               Widget::DeleteOnDestroy);
  frame_container_->SetAlwaysOnTop(true);
  frame_container_->Init(NULL, balloon_rect);
  frame_container_->SetContentsView(this);

  const std::wstring dismiss_text =
      l10n_util::GetString(IDS_NOTIFICATION_BALLOON_DISMISS_LABEL);
  close_button_ = new views::TextButton(close_button_listener_.get(),
                                        dismiss_text);
  close_button_->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::SmallFont));
  close_button_->SetEnabledColor(BrowserThemeProvider::kDefaultColorTabText);
  close_button_->set_alignment(views::TextButton::ALIGN_CENTER);
  close_button_->SetBounds(width() - kDismissButtonWidth
                                   - kRightMargin,
                           height() - kDismissButtonHeight
                                    - kShelfBorderTopOverlap
                                    - kBottomMargin,
                           kDismissButtonWidth,
                           kDismissButtonHeight);
  AddChildView(close_button_);

  const std::wstring options_text =
      l10n_util::GetString(IDS_NOTIFICATION_OPTIONS_MENU_LABEL);
  options_menu_button_ = new views::MenuButton(NULL, options_text, this, false);
  options_menu_button_->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::SmallFont));
  options_menu_button_->SetEnabledColor(
      BrowserThemeProvider::kDefaultColorTabText);
  options_menu_button_->SetBounds(width() - kDismissButtonWidth
                                          - kOptionsMenuWidth
                                          - kRightMargin,
                                  height() - kOptionsMenuHeight
                                           - kShelfBorderTopOverlap
                                           - kBottomMargin,
                                  kOptionsMenuWidth,
                                  kOptionsMenuHeight);
  AddChildView(options_menu_button_);

  const std::wstring source_label_text = l10n_util::GetStringF(
      IDS_NOTIFICATION_BALLOON_SOURCE_LABEL,
      ASCIIToWide(this->balloon_->notification().origin_url().spec()));

  views::Label* source_label = new views::Label(source_label_text);
  source_label->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::SmallFont));
  source_label->SetColor(BrowserThemeProvider::kDefaultColorTabText);
  source_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  source_label->SetBounds(kLeftLabelMargin,
                          height() - kDismissButtonHeight
                                   - kShelfBorderTopOverlap
                                   - kBottomMargin,
                          width() - kDismissButtonWidth
                                  - kOptionsMenuWidth
                                  - kRightMargin,
                          kDismissButtonHeight);
  AddChildView(source_label);

  SizeContentsWindow();
  html_container_->Show();
  frame_container_->Show();

  notification_registrar_.Add(this,
    NotificationType::NOTIFY_BALLOON_DISCONNECTED, Source<Balloon>(balloon));
}

void BalloonViewImpl::RunOptionsMenu(const gfx::Point& pt) {
  CreateOptionsMenu();
  options_menu_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

void BalloonViewImpl::CreateOptionsMenu() {
  if (options_menu_contents_.get())
    return;

  const std::wstring label_text = l10n_util::GetStringF(
      IDS_NOTIFICATION_BALLOON_REVOKE_MESSAGE,
      ASCIIToWide(this->balloon_->notification().origin_url().spec()));

  options_menu_contents_.reset(new views::SimpleMenuModel(this));
  options_menu_contents_->AddItem(kRevokePermissionCommand, label_text);

  options_menu_menu_.reset(new views::Menu2(options_menu_contents_.get()));
}

void BalloonViewImpl::GetContentsMask(const gfx::Rect& rect,
                                      gfx::Path* path) const {
  // This needs to remove areas that look like the following from each corner:
  //
  //   xx
  //   x
  path->moveTo(SkScalar(1), SkScalar(0));
  // Upper right corner
  path->arcTo(rect.width() - SkScalar(2), SkScalar(0),
              rect.width() - SkScalar(1), SkScalar(2),
              SkScalar(1));
  // Lower right corner
  path->arcTo(rect.width() - SkScalar(1), rect.height() - SkScalar(2),
              rect.width() - SkScalar(2), rect.height() - SkScalar(1),
              SkScalar(1));
  // Lower left corner
  path->arcTo(SkScalar(1), rect.height() - SkScalar(1),
              0, rect.height() - SkScalar(2),
              SkScalar(1));
  // Upper left corner
  path->arcTo(0, SkScalar(1), SkScalar(1), 0, SkScalar(1));
}

gfx::Point BalloonViewImpl::GetContentsOffset() const {
  return gfx::Point(kTopShadowWidth + kTopMargin,
                    kLeftShadowWidth + kLeftMargin);
}

int BalloonViewImpl::GetShelfHeight() const {
  // TODO(johnnyg): add scaling here.
  return kDefaultShelfHeight;
}

int BalloonViewImpl::GetFrameWidth() const {
  return size().width() - kLeftShadowWidth - kRightShadowWidth;
}

int BalloonViewImpl::GetTotalFrameHeight() const {
  return size().height() - kTopShadowWidth - kBottomShadowWidth;
}

int BalloonViewImpl::GetBalloonFrameHeight() const {
  return GetTotalFrameHeight() - GetShelfHeight();
}

gfx::Rect BalloonViewImpl::GetContentsRectangle() const {
  if (!frame_container_)
    return gfx::Rect();

  int contents_width = GetFrameWidth() - kLeftMargin - kRightMargin;
  int contents_height = GetBalloonFrameHeight() - kTopMargin - kBottomMargin;
  gfx::Point offset = GetContentsOffset();
  gfx::Rect frame_rect;
  frame_container_->GetBounds(&frame_rect, true);
  return gfx::Rect(frame_rect.x() + offset.x(), frame_rect.y() + offset.y(),
                   contents_width, contents_height);
}

void BalloonViewImpl::Paint(gfx::Canvas* canvas) {
  DCHECK(canvas);
  int background_width = GetFrameWidth();
  int background_height = GetBalloonFrameHeight();
  balloon_background_->Paint(background_width, background_height, canvas);
  canvas->save();
  SkScalar y_offset =
      static_cast<SkScalar>(background_height - kShelfBorderTopOverlap);
  canvas->translate(0, y_offset);
  shelf_background_->Paint(background_width, GetShelfHeight(), canvas);
  canvas->restore();

  View::Paint(canvas);
}

// SimpleMenuModel::Delegate methods
bool BalloonViewImpl::IsCommandIdChecked(int /* command_id */) const {
  // Nothing in the menu is checked.
  return false;
}

bool BalloonViewImpl::IsCommandIdEnabled(int /* command_id */) const {
  // All the menu options are always enabled.
  return true;
}

bool BalloonViewImpl::GetAcceleratorForCommandId(
    int /* command_id */, views::Accelerator* /* accelerator */) {
  // Currently no accelerators.
  return false;
}

void BalloonViewImpl::ExecuteCommand(int command_id) {
  DesktopNotificationService* service =
      balloon_->profile()->GetDesktopNotificationService();
  switch (command_id) {
    case kRevokePermissionCommand:
      service->DenyPermission(balloon_->notification().origin_url());
      break;
    default:
      NOTIMPLEMENTED();
  }
}

void BalloonViewImpl::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (type != NotificationType::NOTIFY_BALLOON_DISCONNECTED) {
    NOTREACHED();
    return;
  }

  // If the renderer process attached to this balloon is disconnected
  // (e.g., because of a crash), we want to close the balloon.
  notification_registrar_.Remove(this,
      NotificationType::NOTIFY_BALLOON_DISCONNECTED, Source<Balloon>(balloon_));
  Close(false);
}
