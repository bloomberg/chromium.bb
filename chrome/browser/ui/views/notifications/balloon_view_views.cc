// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/notifications/balloon_view_views.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_options_menu_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/path.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/notifications/balloon_view_host_chromeos.h"
#else
#include "chrome/browser/ui/views/notifications/balloon_view_host.h"
#endif

namespace {

const int kTopMargin = 2;
const int kBottomMargin = 0;
const int kLeftMargin = 4;
const int kRightMargin = 4;

// Margin between various shelf buttons/label and the shelf border.
const int kShelfMargin = 2;

// Spacing between the options and close buttons.
const int kOptionsDismissSpacing = 4;

// Spacing between the options button and label text.
const int kLabelOptionsSpacing = 4;

// Margin between shelf border and title label.
const int kLabelLeftMargin = 6;

// Size of the drop shadow.  The shadow is provided by BubbleBorder,
// not this class.
const int kLeftShadowWidth = 0;
const int kRightShadowWidth = 0;
const int kTopShadowWidth = 0;
const int kBottomShadowWidth = 6;

// Optional animation.
const bool kAnimateEnabled = true;

// Menu commands
const int kRevokePermissionCommand = 0;

// Colors
const SkColor kControlBarBackgroundColor = SkColorSetRGB(245, 245, 245);
const SkColor kControlBarTextColor = SkColorSetRGB(125, 125, 125);
const SkColor kControlBarSeparatorLineColor = SkColorSetRGB(180, 180, 180);

}  // namespace

// static
int BalloonView::GetHorizontalMargin() {
  return kLeftMargin + kRightMargin + kLeftShadowWidth + kRightShadowWidth;
}

BalloonViewImpl::BalloonViewImpl(BalloonCollection* collection)
    : balloon_(NULL),
      collection_(collection),
      frame_container_(NULL),
      html_container_(NULL),
      html_contents_(NULL),
      close_button_(NULL),
      animation_(NULL),
      options_menu_model_(NULL),
      options_menu_button_(NULL),
      enable_web_ui_(false),
      closed_by_user_(false) {
  // We're owned by Balloon and don't want to be deleted by our parent View.
  set_owned_by_client();

  set_border(new views::BubbleBorder(views::BubbleBorder::FLOAT,
      views::BubbleBorder::NO_SHADOW, SK_ColorWHITE));
}

BalloonViewImpl::~BalloonViewImpl() {
}

void BalloonViewImpl::Close(bool by_user) {
  animation_->Stop();
  html_contents_->Shutdown();
  // Detach contents from the widget before they close.
  // This is necessary because a widget may be deleted
  // after this when chrome is shutting down.
  html_container_->GetRootView()->RemoveAllChildViews(true);
  html_container_->Close();
  frame_container_->GetRootView()->RemoveAllChildViews(true);
  frame_container_->Close();
  closed_by_user_ = by_user;
  // |frame_container_->::Close()| is async. When processed it'll call back to
  // DeleteDelegate() and we'll cleanup.
}

gfx::Size BalloonViewImpl::GetSize() const {
  // BalloonView has no size if it hasn't been shown yet (which is when
  // balloon_ is set).
  if (!balloon_)
    return gfx::Size(0, 0);

  return gfx::Size(GetTotalWidth(), GetTotalHeight());
}

BalloonHost* BalloonViewImpl::GetHost() const {
  return html_contents_.get();
}

void BalloonViewImpl::OnMenuButtonClicked(views::View* source,
                                          const gfx::Point& point) {
  CreateOptionsMenu();

  menu_runner_.reset(new views::MenuRunner(options_menu_model_.get()));

  gfx::Point screen_location;
  views::View::ConvertPointToScreen(options_menu_button_, &screen_location);
  if (menu_runner_->RunMenuAt(
          source->GetWidget()->GetTopLevelWidget(),
          options_menu_button_,
          gfx::Rect(screen_location, options_menu_button_->size()),
          views::MenuItemView::TOPRIGHT,
          views::MenuRunner::HAS_MNEMONICS) == views::MenuRunner::MENU_DELETED)
    return;
}

void BalloonViewImpl::OnDisplayChanged() {
  collection_->DisplayChanged();
}

void BalloonViewImpl::OnWorkAreaChanged() {
  collection_->DisplayChanged();
}

void BalloonViewImpl::DeleteDelegate() {
  balloon_->OnClose(closed_by_user_);
}

void BalloonViewImpl::ButtonPressed(views::Button* sender, const ui::Event&) {
  // The only button currently is the close button.
  DCHECK_EQ(close_button_, sender);
  Close(true);
}

gfx::Size BalloonViewImpl::GetPreferredSize() {
  return gfx::Size(1000, 1000);
}

void BalloonViewImpl::SizeContentsWindow() {
  if (!html_container_ || !frame_container_)
    return;

  gfx::Rect contents_rect = GetContentsRectangle();
  html_container_->SetBounds(contents_rect);
  html_container_->StackAboveWidget(frame_container_);

  gfx::Path path;
  GetContentsMask(contents_rect, &path);
  html_container_->SetShape(path.CreateNativeRegion());

  close_button_->SetBoundsRect(GetCloseButtonBounds());
  options_menu_button_->SetBoundsRect(GetOptionsButtonBounds());
  source_label_->SetBoundsRect(GetLabelBounds());
}

void BalloonViewImpl::RepositionToBalloon() {
  DCHECK(frame_container_);
  DCHECK(html_container_);
  DCHECK(balloon_);

  if (!kAnimateEnabled) {
    frame_container_->SetBounds(GetBoundsForFrameContainer());
    gfx::Rect contents_rect = GetContentsRectangle();
    html_container_->SetBounds(contents_rect);
    html_contents_->SetPreferredSize(contents_rect.size());
    content::RenderWidgetHostView* view =
        html_contents_->web_contents()->GetRenderWidgetHostView();
    if (view)
      view->SetSize(contents_rect.size());
    return;
  }

  anim_frame_end_ = GetBoundsForFrameContainer();
  anim_frame_start_ = frame_container_->GetClientAreaBoundsInScreen();
  animation_.reset(new ui::SlideAnimation(this));
  animation_->Show();
}

void BalloonViewImpl::Update() {
  // Tls might get called before html_contents_ is set in Show() if more than
  // one update with the same replace_id occurs, or if an update occurs after
  // the ballon has been closed (e.g. during shutdown) but before this has been
  // destroyed.
  if (!html_contents_.get() || !html_contents_->web_contents())
    return;
  html_contents_->web_contents()->GetController().LoadURL(
      balloon_->notification().content_url(), content::Referrer(),
      content::PAGE_TRANSITION_LINK, std::string());
}

void BalloonViewImpl::AnimationProgressed(const ui::Animation* animation) {
  DCHECK_EQ(animation_.get(), animation);

  // Linear interpolation from start to end position.
  gfx::Rect frame_position(animation_->CurrentValueBetween(
                               anim_frame_start_, anim_frame_end_));
  frame_container_->SetBounds(frame_position);

  gfx::Path path;
  gfx::Rect contents_rect = GetContentsRectangle();
  html_container_->SetBounds(contents_rect);
  GetContentsMask(contents_rect, &path);
  html_container_->SetShape(path.CreateNativeRegion());

  html_contents_->SetPreferredSize(contents_rect.size());
  content::RenderWidgetHostView* view =
      html_contents_->web_contents()->GetRenderWidgetHostView();
  if (view)
    view->SetSize(contents_rect.size());
}

gfx::Rect BalloonViewImpl::GetCloseButtonBounds() const {
  gfx::Rect bounds(GetContentsBounds());
  bounds.set_height(GetShelfHeight());
  const gfx::Size& pref_size(close_button_->GetPreferredSize());
  bounds.Inset(bounds.width() - kShelfMargin - pref_size.width(), 0,
      kShelfMargin, 0);
  bounds.ClampToCenteredSize(pref_size);
  return bounds;
}

gfx::Rect BalloonViewImpl::GetOptionsButtonBounds() const {
  gfx::Rect bounds(GetContentsBounds());
  bounds.set_height(GetShelfHeight());
  const gfx::Size& pref_size(options_menu_button_->GetPreferredSize());
  bounds.set_x(GetCloseButtonBounds().x() - kOptionsDismissSpacing -
      pref_size.width());
  bounds.set_width(pref_size.width());
  bounds.ClampToCenteredSize(pref_size);
  return bounds;
}

gfx::Rect BalloonViewImpl::GetLabelBounds() const {
  gfx::Rect bounds(GetContentsBounds());
  bounds.set_height(GetShelfHeight());
  gfx::Size pref_size(source_label_->GetPreferredSize());
  bounds.Inset(kLabelLeftMargin, 0, bounds.width() -
      GetOptionsButtonBounds().x() + kLabelOptionsSpacing, 0);
  pref_size.set_width(bounds.width());
  bounds.ClampToCenteredSize(pref_size);
  return bounds;
}

void BalloonViewImpl::Show(Balloon* balloon) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  balloon_ = balloon;

  const string16 source_label_text = l10n_util::GetStringFUTF16(
      IDS_NOTIFICATION_BALLOON_SOURCE_LABEL,
      balloon->notification().display_source());

  source_label_ = new views::Label(source_label_text);
  AddChildView(source_label_);
  options_menu_button_ = new views::MenuButton(NULL, string16(), this, false);
  AddChildView(options_menu_button_);
#if defined(OS_CHROMEOS)
  // Disable and hide the options menu on ChromeOS. This is a short term fix
  // for a crash (long term we're redesigning notifications).
  options_menu_button_->SetEnabled(false);
  options_menu_button_->SetVisible(false);
#endif
  close_button_ = new views::ImageButton(this);
  close_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_NOTIFICATION_BALLOON_DISMISS_LABEL));
  AddChildView(close_button_);

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
  //
  // We don't let the OS manage the RTL layout of these widgets, because
  // this code is already taking care of correctly reversing the layout.
#if defined(OS_CHROMEOS) && defined(USE_AURA)
  html_contents_.reset(new chromeos::BalloonViewHost(balloon));
#else
  html_contents_.reset(new BalloonViewHost(balloon));
#endif
  html_contents_->SetPreferredSize(gfx::Size(10000, 10000));
  if (enable_web_ui_)
    html_contents_->EnableWebUI();

  html_container_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  html_container_->Init(params);
  html_container_->SetContentsView(html_contents_->view());

  frame_container_ = new views::Widget;
  params.delegate = this;
  params.transparent = true;
  params.bounds = GetBoundsForFrameContainer();
  frame_container_->Init(params);
  frame_container_->SetContentsView(this);
  frame_container_->StackAboveWidget(html_container_);

  // GetContentsRectangle() is calculated relative to |frame_container_|. Make
  // sure |frame_container_| has bounds before we ask for
  // GetContentsRectangle().
  html_container_->SetBounds(GetContentsRectangle());

  // SetAlwaysOnTop should be called after StackAboveWidget because otherwise
  // the top-most flag will be removed.
  html_container_->SetAlwaysOnTop(true);
  frame_container_->SetAlwaysOnTop(true);

  close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                          rb.GetImageSkiaNamed(IDR_CLOSE_1));
  close_button_->SetImage(views::CustomButton::STATE_HOVERED,
                          rb.GetImageSkiaNamed(IDR_CLOSE_1_H));
  close_button_->SetImage(views::CustomButton::STATE_PRESSED,
                          rb.GetImageSkiaNamed(IDR_CLOSE_1_P));
  close_button_->SetBoundsRect(GetCloseButtonBounds());
  close_button_->SetBackground(SK_ColorBLACK,
                               rb.GetImageSkiaNamed(IDR_CLOSE_1),
                               rb.GetImageSkiaNamed(IDR_CLOSE_1_MASK));

  options_menu_button_->SetIcon(*rb.GetImageSkiaNamed(IDR_BALLOON_WRENCH));
  options_menu_button_->SetHoverIcon(
      *rb.GetImageSkiaNamed(IDR_BALLOON_WRENCH_H));
  options_menu_button_->SetPushedIcon(*rb.GetImageSkiaNamed(
      IDR_BALLOON_WRENCH_P));
  options_menu_button_->set_alignment(views::TextButton::ALIGN_CENTER);
  options_menu_button_->set_border(NULL);
  options_menu_button_->SetBoundsRect(GetOptionsButtonBounds());

  source_label_->SetFont(rb.GetFont(ui::ResourceBundle::SmallFont));
  source_label_->SetBackgroundColor(kControlBarBackgroundColor);
  source_label_->SetEnabledColor(kControlBarTextColor);
  source_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  source_label_->SetElideBehavior(views::Label::ELIDE_AT_END);
  source_label_->SetBoundsRect(GetLabelBounds());

  SizeContentsWindow();
  html_container_->Show();
  frame_container_->Show();

  notification_registrar_.Add(
    this, chrome::NOTIFICATION_NOTIFY_BALLOON_DISCONNECTED,
    content::Source<Balloon>(balloon));
}

void BalloonViewImpl::CreateOptionsMenu() {
  if (options_menu_model_.get())
    return;
  options_menu_model_.reset(new NotificationOptionsMenuModel(balloon_));
}

void BalloonViewImpl::GetContentsMask(const gfx::Rect& rect,
                                      gfx::Path* path) const {
  // This rounds the corners, and we also cut out a circle for the close
  // button, since we can't guarantee the ordering of two top-most windows.
  SkScalar radius = SkIntToScalar(views::BubbleBorder::GetCornerRadius());
  SkScalar spline_radius = radius -
      SkScalarMul(radius, (SK_ScalarSqrt2 - SK_Scalar1) * 4 / 3);
  SkScalar left = SkIntToScalar(0);
  SkScalar top = SkIntToScalar(0);
  SkScalar right = SkIntToScalar(rect.width());
  SkScalar bottom = SkIntToScalar(rect.height());

  path->moveTo(left, top);
  path->lineTo(right, top);
  path->lineTo(right, bottom - radius);
  path->cubicTo(right, bottom - spline_radius,
                right - spline_radius, bottom,
                right - radius, bottom);
  path->lineTo(left + radius, bottom);
  path->cubicTo(left + spline_radius, bottom,
                left, bottom - spline_radius,
                left, bottom - radius);
  path->lineTo(left, top);
  path->close();
}

void BalloonViewImpl::GetFrameMask(const gfx::Rect& rect,
                                   gfx::Path* path) const {
  SkScalar radius = SkIntToScalar(views::BubbleBorder::GetCornerRadius());
  SkScalar spline_radius = radius -
      SkScalarMul(radius, (SK_ScalarSqrt2 - SK_Scalar1) * 4 / 3);
  SkScalar left = SkIntToScalar(rect.x());
  SkScalar top = SkIntToScalar(rect.y());
  SkScalar right = SkIntToScalar(rect.right());
  SkScalar bottom = SkIntToScalar(rect.bottom());

  path->moveTo(left, bottom);
  path->lineTo(left, top + radius);
  path->cubicTo(left, top + spline_radius,
                left + spline_radius, top,
                left + radius, top);
  path->lineTo(right - radius, top);
  path->cubicTo(right - spline_radius, top,
                right, top + spline_radius,
                right, top + radius);
  path->lineTo(right, bottom);
  path->lineTo(left, bottom);
  path->close();
}

gfx::Point BalloonViewImpl::GetContentsOffset() const {
  return gfx::Point(kLeftShadowWidth + kLeftMargin,
                    kTopShadowWidth + kTopMargin);
}

gfx::Rect BalloonViewImpl::GetBoundsForFrameContainer() const {
  return gfx::Rect(balloon_->GetPosition().x(), balloon_->GetPosition().y(),
                   GetTotalWidth(), GetTotalHeight());
}

int BalloonViewImpl::GetShelfHeight() const {
  // TODO(johnnyg): add scaling here.
  int max_button_height = std::max(std::max(
      close_button_->GetPreferredSize().height(),
      options_menu_button_->GetPreferredSize().height()),
      source_label_->GetPreferredSize().height());
  return max_button_height + kShelfMargin * 2;
}

int BalloonViewImpl::GetBalloonFrameHeight() const {
  return GetTotalHeight() - GetShelfHeight();
}

int BalloonViewImpl::GetTotalWidth() const {
  return balloon_->content_size().width() +
      kLeftMargin + kRightMargin + kLeftShadowWidth + kRightShadowWidth;
}

int BalloonViewImpl::GetTotalHeight() const {
  return balloon_->content_size().height() +
      kTopMargin + kBottomMargin + kTopShadowWidth + kBottomShadowWidth +
      GetShelfHeight();
}

gfx::Rect BalloonViewImpl::GetContentsRectangle() const {
  if (!frame_container_)
    return gfx::Rect();

  gfx::Size content_size = balloon_->content_size();
  gfx::Point offset = GetContentsOffset();
  gfx::Rect frame_rect = frame_container_->GetWindowBoundsInScreen();
  return gfx::Rect(frame_rect.x() + offset.x(),
                   frame_rect.y() + GetShelfHeight() + offset.y(),
                   content_size.width(),
                   content_size.height());
}

void BalloonViewImpl::OnPaint(gfx::Canvas* canvas) {
  DCHECK(canvas);
  // Paint the menu bar area white, with proper rounded corners.
  gfx::Path path;
  gfx::Rect rect = GetContentsBounds();
  rect.set_height(GetShelfHeight());
  GetFrameMask(rect, &path);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(kControlBarBackgroundColor);
  canvas->DrawPath(path, paint);

  // Draw a 1-pixel gray line between the content and the menu bar.
  int line_width = GetTotalWidth() - kLeftMargin - kRightMargin;
  canvas->FillRect(gfx::Rect(kLeftMargin, rect.bottom(), line_width, 1),
                   kControlBarSeparatorLineColor);
  View::OnPaint(canvas);
  OnPaintBorder(canvas);
}

void BalloonViewImpl::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SizeContentsWindow();
}

void BalloonViewImpl::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_NOTIFY_BALLOON_DISCONNECTED) {
    NOTREACHED();
    return;
  }

  // If the renderer process attached to this balloon is disconnected
  // (e.g., because of a crash), we want to close the balloon.
  notification_registrar_.Remove(
      this, chrome::NOTIFICATION_NOTIFY_BALLOON_DISCONNECTED,
      content::Source<Balloon>(balloon_));
  Close(false);
}
