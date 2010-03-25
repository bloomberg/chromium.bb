// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/notifications/balloon_view_gtk.h"

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/notifications/balloon_view_host_gtk.h"
#include "chrome/browser/gtk/notifications/notification_options_menu_model.h"
#include "chrome/browser/gtk/rounded_window.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "gfx/canvas.h"
#include "gfx/gtk_util.h"
#include "gfx/insets.h"
#include "gfx/native_widget_types.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// Margin, in pixels, between the notification frame and the contents
// of the notification.
const int kTopMargin = 2;
const int kBottomMargin = 1;
const int kLeftMargin = 2;
const int kRightMargin = 2;

// How many pixels of overlap there is between the shelf top and the
// balloon bottom.
const int kShelfBorderTopOverlap = 3;

// Properties of the dismiss button.
const int kDismissButtonWidth = 60;
const int kDismissButtonHeight = 20;

// Properties of the options menu.
const int kOptionsMenuWidth = 60;
const int kOptionsMenuHeight = 20;

// Properties of the origin label.
const int kLeftLabelMargin = 5;

// TODO(johnnyg): Add a shadow for the frame.
const int kLeftShadowWidth = 0;
const int kRightShadowWidth = 0;
const int kTopShadowWidth = 0;
const int kBottomShadowWidth = 0;

// Space in pixels between text and icon on the buttons.
const int kButtonIconSpacing = 3;

// Number of characters to show in the origin label before ellipsis.
const int kOriginLabelCharacters = 18;

// The shelf height for the system default font size.  It is scaled
// with changes in the default font size.
const int kDefaultShelfHeight = 24;

// Makes the website label relatively smaller to the base text size.
const char* kLabelMarkup = "<span size=\"smaller\">%s</span>";

}  // namespace

BalloonViewImpl::BalloonViewImpl(BalloonCollection* collection)
    : balloon_(NULL),
      frame_container_(NULL),
      html_container_(NULL),
      html_contents_(NULL),
      method_factory_(this),
      close_button_(NULL),
      animation_(NULL) {
  // Load the sprites for the frames.
  // Insets are such because the sprites have 3x3 corners.
  shelf_background_.reset(new NineBox(IDR_BALLOON_SHELF, 3, 3, 3, 3));
  balloon_background_.reset(new NineBox(IDR_BALLOON_BORDER, 3, 3, 3, 3));
}

BalloonViewImpl::~BalloonViewImpl() {
}

void BalloonViewImpl::Close(bool by_user) {
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &BalloonViewImpl::DelayedClose, by_user));
}

gfx::Size BalloonViewImpl::GetSize() const {
  // BalloonView has no size if it hasn't been shown yet (which is when
  // balloon_ is set).
  if (!balloon_)
    return gfx::Size();

  // Although this may not be the instantaneous size of the balloon if
  // called in the middle of an animation, it is the effective size that
  // will result from the animation.
  return gfx::Size(GetDesiredTotalWidth(), GetDesiredTotalHeight());
}

void BalloonViewImpl::DelayedClose(bool by_user) {
  html_contents_->Shutdown();
  gtk_widget_hide(frame_container_);
  balloon_->OnClose(by_user);
}

void BalloonViewImpl::RepositionToBalloon() {
  DCHECK(frame_container_);
  DCHECK(balloon_);

  // Create an amination from the current position to the desired one.
  int start_x;
  int start_y;
  int start_w;
  int start_h;
  gtk_window_get_position(GTK_WINDOW(frame_container_), &start_x, &start_y);
  gtk_window_get_size(GTK_WINDOW(frame_container_), &start_w, &start_h);

  int end_x = balloon_->position().x();
  int end_y = balloon_->position().y();
  int end_w = GetDesiredTotalWidth();
  int end_h = GetDesiredTotalHeight();

  anim_frame_start_ = gfx::Rect(start_x, start_y, start_w, start_h);
  anim_frame_end_ = gfx::Rect(end_x, end_y, end_w, end_h);
  animation_.reset(new SlideAnimation(this));
  animation_->Show();
}

void BalloonViewImpl::AnimationProgressed(const Animation* animation) {
  DCHECK_EQ(animation, animation_.get());

  // Linear interpolation from start to end position.
  double end = animation->GetCurrentValue();
  double start = 1.0 - end;

  gfx::Rect frame_position(
      static_cast<int>(start * anim_frame_start_.x() +
                       end * anim_frame_end_.x()),
      static_cast<int>(start * anim_frame_start_.y() +
                       end * anim_frame_end_.y()),
      static_cast<int>(start * anim_frame_start_.width() +
                       end * anim_frame_end_.width()),
      static_cast<int>(start * anim_frame_start_.height() +
                       end * anim_frame_end_.height()));
  gtk_window_resize(GTK_WINDOW(frame_container_),
                    frame_position.width(), frame_position.height());
  gtk_window_move(GTK_WINDOW(frame_container_),
                  frame_position.x(), frame_position.y());

  gfx::Rect contents_rect = GetContentsRectangle();
  html_contents_->UpdateActualSize(contents_rect.size());
}

void PrepareButtonWithIcon(GtkWidget* button,
                           const std::string& text_utf8, int icon_id) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* pixbuf = rb.GetPixbufNamed(icon_id);
  GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
  g_object_unref(pixbuf);

  GtkWidget* box = gtk_hbox_new(FALSE, kButtonIconSpacing);

  GtkWidget* label = gtk_label_new(text_utf8.c_str());
  gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);

  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 1, 1, 1, 1);
  gtk_container_add(GTK_CONTAINER(alignment), box);
  gtk_container_add(GTK_CONTAINER(button), alignment);

  gtk_widget_show_all(alignment);
}

void BalloonViewImpl::Show(Balloon* balloon) {
  GtkThemeProvider* theme_provider = GtkThemeProvider::GetFrom(
      balloon->profile());

  const std::string source_label_text = l10n_util::GetStringFUTF8(
      IDS_NOTIFICATION_BALLOON_SOURCE_LABEL,
      WideToUTF16(balloon->notification().display_source()));
  const std::string options_text =
      l10n_util::GetStringUTF8(IDS_NOTIFICATION_OPTIONS_MENU_LABEL);
  const std::string dismiss_text =
      l10n_util::GetStringUTF8(IDS_NOTIFICATION_BALLOON_DISMISS_LABEL);

  balloon_ = balloon;
  frame_container_ = gtk_window_new(GTK_WINDOW_POPUP);

  // Construct the options menu.
  options_menu_model_.reset(new NotificationOptionsMenuModel(balloon_));
  options_menu_.reset(new MenuGtk(this, options_menu_model_.get()));

  // Create a BalloonViewHost to host the HTML contents of this balloon.
  html_contents_ = new BalloonViewHost(balloon);
  html_contents_->Init();
  gfx::NativeView contents = html_contents_->native_view();

  gtk_widget_set_app_paintable(frame_container_, TRUE);
  gtk_widget_realize(frame_container_);

  // Divide the frame vertically into the content area and the shelf.
  GtkWidget* vbox = gtk_vbox_new(0, 0);
  gtk_container_add(GTK_CONTAINER(frame_container_), vbox);

  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(
      GTK_ALIGNMENT(alignment),
      kTopMargin, kBottomMargin, kLeftMargin, kRightMargin);
  gtk_widget_show_all(alignment);
  gtk_container_add(GTK_CONTAINER(alignment), contents);
  gtk_container_add(GTK_CONTAINER(vbox), alignment);

  shelf_ = gtk_hbox_new(0, 0);
  GtkWidget* alignment2 = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment2), 0, 0, 10, 0);
  gtk_container_add(GTK_CONTAINER(vbox), shelf_);

  // Create a toolbar and add it to the shelf.
  hbox_ = gtk_hbox_new(FALSE, 0);
  //  gtk_util::SuppressDefaultPainting(hbox_);
  gtk_widget_set_size_request(GTK_WIDGET(hbox_), -1, GetShelfHeight());
  gtk_container_add(GTK_CONTAINER(alignment2), hbox_);
  gtk_container_add(GTK_CONTAINER(shelf_), alignment2);
  gtk_widget_show_all(vbox);

  g_signal_connect(frame_container_, "expose-event",
                   G_CALLBACK(HandleExposeThunk), this);

  // Create a label for the source of the notification and add it to the
  // toolbar.
  GtkWidget* source_label_ = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(kLabelMarkup,
                                         source_label_text.c_str());
  gtk_label_set_markup(GTK_LABEL(source_label_), markup);
  g_free(markup);
  gtk_label_set_max_width_chars(GTK_LABEL(source_label_),
                                kOriginLabelCharacters);
  gtk_label_set_ellipsize(GTK_LABEL(source_label_), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start(GTK_BOX(hbox_), source_label_, FALSE, FALSE, 0);

  // Create a button for showing the options menu, and add it to the toolbar.
  options_menu_button_ = theme_provider->BuildChromeButton();
  g_signal_connect(options_menu_button_, "clicked",
                   G_CALLBACK(HandleOptionsMenuButtonThunk), this);
  PrepareButtonWithIcon(options_menu_button_, options_text,
                        IDR_BALLOON_OPTIONS_ARROW_HOVER);
  gtk_box_pack_start(GTK_BOX(hbox_), options_menu_button_, FALSE, FALSE, 0);

  // Create a button to dismiss the balloon and add it to the toolbar.
  close_button_ = theme_provider->BuildChromeButton();
  g_signal_connect(close_button_, "clicked",
                   G_CALLBACK(HandleCloseButtonThunk), this);
  PrepareButtonWithIcon(close_button_, dismiss_text, IDR_BALLOON_CLOSE_HOVER);
  gtk_box_pack_start(GTK_BOX(hbox_), close_button_, FALSE, FALSE, 0);

  // Position the view elements according to the balloon position and show.
  RepositionToBalloon();
  gtk_widget_show_all(GTK_WIDGET(hbox_));
  gtk_widget_show(frame_container_);

  gtk_util::ActAsRoundedWindow(frame_container_, gfx::kGdkBlack, 3,
                               gtk_util::ROUNDED_ALL,
                               gtk_util::BORDER_ALL);

  notification_registrar_.Add(this,
      NotificationType::NOTIFY_BALLOON_DISCONNECTED, Source<Balloon>(balloon));
}

void BalloonViewImpl::RunOptionsMenu() {
  options_menu_->PopupAsContext(gtk_get_current_event_time());
}

gfx::Point BalloonViewImpl::GetContentsOffset() const {
  return gfx::Point(kTopShadowWidth + kTopMargin,
                    kLeftShadowWidth + kLeftMargin);
}

int BalloonViewImpl::GetShelfHeight() const {
  // TODO(johnnyg): add scaling here.
  return kDefaultShelfHeight;
}

int BalloonViewImpl::GetBalloonFrameHeight() const {
  return GetDesiredTotalHeight() - GetShelfHeight();
}

int BalloonViewImpl::GetDesiredTotalWidth() const {
  return balloon_->content_size().width() +
      kLeftMargin + kRightMargin + kLeftShadowWidth + kRightShadowWidth;
}

int BalloonViewImpl::GetDesiredTotalHeight() const {
  return balloon_->content_size().height() +
      kTopMargin + kBottomMargin + kTopShadowWidth + kBottomShadowWidth +
      GetShelfHeight();
}

gfx::Rect BalloonViewImpl::GetContentsRectangle() const {
  if (!frame_container_)
    return gfx::Rect();

  gfx::Size content_size = balloon_->content_size();
  gfx::Point offset = GetContentsOffset();
  int x = 0, y = 0;
  gtk_window_get_position(GTK_WINDOW(frame_container_), &x, &y);
  return gfx::Rect(x + offset.x(), y + offset.y(),
                   content_size.width(), content_size.height());
}

gboolean BalloonViewImpl::HandleExpose() {
  // Draw the background images.
  balloon_background_->RenderToWidget(frame_container_);
  shelf_background_->RenderToWidget(shelf_);
  return FALSE;
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
