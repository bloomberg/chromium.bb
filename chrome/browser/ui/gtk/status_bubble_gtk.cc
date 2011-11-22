// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/status_bubble_gtk.h"

#include <gtk/gtk.h>

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/rounded_window.h"
#include "chrome/browser/ui/gtk/slide_animator_gtk.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/text/text_elider.h"

namespace {

// Inner padding between the border and the text label.
const int kInternalTopBottomPadding = 1;
const int kInternalLeftRightPadding = 2;

// The radius of the edges of our bubble.
const int kCornerSize = 3;

// Milliseconds before we hide the status bubble widget when you mouseout.
const int kHideDelay = 250;

// How close the mouse can get to the infobubble before it starts sliding
// off-screen.
const int kMousePadding = 20;

}  // namespace

StatusBubbleGtk::StatusBubbleGtk(Profile* profile)
    : theme_service_(GtkThemeService::GetFrom(profile)),
      padding_(NULL),
      flip_horizontally_(false),
      y_offset_(0),
      download_shelf_is_visible_(false),
      last_mouse_location_(0, 0),
      last_mouse_left_content_(false),
      ignore_next_left_content_(false) {
  InitWidgets();

  theme_service_->InitThemesFor(this);
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
}

StatusBubbleGtk::~StatusBubbleGtk() {
  label_.Destroy();
  container_.Destroy();
}

void StatusBubbleGtk::SetStatus(const string16& status_text_wide) {
  std::string status_text = UTF16ToUTF8(status_text_wide);
  if (status_text_ == status_text)
    return;

  status_text_ = status_text;
  if (!status_text_.empty())
    SetStatusTextTo(status_text_);
  else if (!url_text_.empty())
    SetStatusTextTo(url_text_);
  else
    SetStatusTextTo(std::string());
}

void StatusBubbleGtk::SetURL(const GURL& url, const std::string& languages) {
  url_ = url;
  languages_ = languages;

  // If we want to clear a displayed URL but there is a status still to
  // display, display that status instead.
  if (url.is_empty() && !status_text_.empty()) {
    url_text_ = std::string();
    SetStatusTextTo(status_text_);
    return;
  }

  SetStatusTextToURL();
}

void StatusBubbleGtk::SetStatusTextToURL() {
  GtkWidget* parent = gtk_widget_get_parent(container_.get());

  // It appears that parent can be NULL (probably only during shutdown).
  if (!parent || !gtk_widget_get_realized(parent))
    return;

  int desired_width = parent->allocation.width;
  if (!expanded()) {
    expand_timer_.Stop();
    expand_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(kExpandHoverDelay),
                        this, &StatusBubbleGtk::ExpandURL);
    // When not expanded, we limit the size to one third the browser's
    // width.
    desired_width /= 3;
  }

  // TODO(tc): We don't actually use gfx::Font as the font in the status
  // bubble.  We should extend ui::ElideUrl to take some sort of pango font.
  url_text_ = UTF16ToUTF8(
      ui::ElideUrl(url_, gfx::Font(), desired_width, languages_));
  SetStatusTextTo(url_text_);
}

void StatusBubbleGtk::Show() {
  // If we were going to hide, stop.
  hide_timer_.Stop();

  gtk_widget_show(container_.get());
  if (container_->window)
    gdk_window_raise(container_->window);
}

void StatusBubbleGtk::Hide() {
  // If we were going to expand the bubble, stop.
  expand_timer_.Stop();
  expand_animation_.reset();

  gtk_widget_hide(container_.get());
}

void StatusBubbleGtk::SetStatusTextTo(const std::string& status_utf8) {
  if (status_utf8.empty()) {
    hide_timer_.Stop();
    hide_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kHideDelay),
                      this, &StatusBubbleGtk::Hide);
  } else {
    gtk_label_set_text(GTK_LABEL(label_.get()), status_utf8.c_str());
    GtkRequisition req;
    gtk_widget_size_request(label_.get(), &req);
    desired_width_ = req.width;

    UpdateLabelSizeRequest();

    if (!last_mouse_left_content_) {
      // Show the padding and label to update our requisition and then
      // re-process the last mouse event -- if the label was empty before or the
      // text changed, our size will have changed and we may need to move
      // ourselves away from the pointer now.
      gtk_widget_show_all(padding_);
      MouseMoved(last_mouse_location_, false);
    }
    Show();
  }
}

void StatusBubbleGtk::MouseMoved(
    const gfx::Point& location, bool left_content) {
  if (left_content && ignore_next_left_content_) {
    ignore_next_left_content_ = false;
    return;
  }

  last_mouse_location_ = location;
  last_mouse_left_content_ = left_content;

  if (!gtk_widget_get_realized(container_.get()))
    return;

  GtkWidget* parent = gtk_widget_get_parent(container_.get());
  if (!parent || !gtk_widget_get_realized(parent))
    return;

  int old_y_offset = y_offset_;
  bool old_flip_horizontally = flip_horizontally_;

  if (left_content) {
    SetFlipHorizontally(false);
    y_offset_ = 0;
  } else {
    GtkWidget* toplevel = gtk_widget_get_toplevel(container_.get());
    if (!toplevel || !gtk_widget_get_realized(toplevel))
      return;

    bool ltr = !base::i18n::IsRTL();

    GtkRequisition requisition;
    gtk_widget_size_request(container_.get(), &requisition);

    // Get our base position (that is, not including the current offset)
    // relative to the origin of the root window.
    gint toplevel_x = 0, toplevel_y = 0;
    gdk_window_get_position(toplevel->window, &toplevel_x, &toplevel_y);
    gfx::Rect parent_rect =
        gtk_util::GetWidgetRectRelativeToToplevel(parent);
    gfx::Rect bubble_rect(
        toplevel_x + parent_rect.x() +
            (ltr ? 0 : parent->allocation.width - requisition.width),
        toplevel_y + parent_rect.y() +
            parent->allocation.height - requisition.height,
        requisition.width,
        requisition.height);

    int left_threshold =
        bubble_rect.x() - bubble_rect.height() - kMousePadding;
    int right_threshold =
        bubble_rect.right() + bubble_rect.height() + kMousePadding;
    int top_threshold = bubble_rect.y() - kMousePadding;

    if (((ltr && location.x() < right_threshold) ||
         (!ltr && location.x() > left_threshold)) &&
        location.y() > top_threshold) {
      if (download_shelf_is_visible_) {
        SetFlipHorizontally(true);
        y_offset_ = 0;
      } else {
        SetFlipHorizontally(false);
        int distance = std::max(ltr ?
                                    location.x() - right_threshold :
                                    left_threshold - location.x(),
                                top_threshold - location.y());
        y_offset_ = std::min(-1 * distance, requisition.height);
      }
    } else {
      SetFlipHorizontally(false);
      y_offset_ = 0;
    }
  }

  if (y_offset_ != old_y_offset || flip_horizontally_ != old_flip_horizontally)
    gtk_widget_queue_resize_no_redraw(parent);
}

void StatusBubbleGtk::UpdateDownloadShelfVisibility(bool visible) {
  download_shelf_is_visible_ = visible;
}

void StatusBubbleGtk::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_BROWSER_THEME_CHANGED) {
    UserChangedTheme();
  }
}

void StatusBubbleGtk::InitWidgets() {
  bool ltr = !base::i18n::IsRTL();

  label_.Own(gtk_label_new(NULL));

  padding_ = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(padding_),
      kInternalTopBottomPadding, kInternalTopBottomPadding,
      kInternalLeftRightPadding + (ltr ? 0 : kCornerSize),
      kInternalLeftRightPadding + (ltr ? kCornerSize : 0));
  gtk_container_add(GTK_CONTAINER(padding_), label_.get());
  gtk_widget_show_all(padding_);

  container_.Own(gtk_event_box_new());
  gtk_widget_set_no_show_all(container_.get(), TRUE);
  gtk_util::ActAsRoundedWindow(
      container_.get(), ui::kGdkWhite, kCornerSize,
      gtk_util::ROUNDED_TOP_RIGHT,
      gtk_util::BORDER_TOP | gtk_util::BORDER_RIGHT);
  gtk_widget_set_name(container_.get(), "status-bubble");
  gtk_container_add(GTK_CONTAINER(container_.get()), padding_);

  // We need to listen for mouse motion events, since a fast-moving pointer may
  // enter our window without us getting any motion events on the browser near
  // enough for us to run away.
  gtk_widget_add_events(container_.get(), GDK_POINTER_MOTION_MASK |
                                          GDK_ENTER_NOTIFY_MASK);
  g_signal_connect(container_.get(), "motion-notify-event",
                   G_CALLBACK(HandleMotionNotifyThunk), this);
  g_signal_connect(container_.get(), "enter-notify-event",
                   G_CALLBACK(HandleEnterNotifyThunk), this);

  UserChangedTheme();
}

void StatusBubbleGtk::UserChangedTheme() {
  if (theme_service_->UsingNativeTheme()) {
    gtk_widget_modify_fg(label_.get(), GTK_STATE_NORMAL, NULL);
    gtk_widget_modify_bg(container_.get(), GTK_STATE_NORMAL, NULL);
  } else {
    // TODO(erg): This is the closest to "text that will look good on a
    // toolbar" that I can find. Maybe in later iterations of the theme system,
    // there will be a better color to pick.
    GdkColor bookmark_text =
        theme_service_->GetGdkColor(ThemeService::COLOR_BOOKMARK_TEXT);
    gtk_widget_modify_fg(label_.get(), GTK_STATE_NORMAL, &bookmark_text);

    GdkColor toolbar_color =
        theme_service_->GetGdkColor(ThemeService::COLOR_TOOLBAR);
    gtk_widget_modify_bg(container_.get(), GTK_STATE_NORMAL, &toolbar_color);
  }

  gtk_util::SetRoundedWindowBorderColor(container_.get(),
                                        theme_service_->GetBorderColor());
}

void StatusBubbleGtk::SetFlipHorizontally(bool flip_horizontally) {
  if (flip_horizontally == flip_horizontally_)
    return;

  flip_horizontally_ = flip_horizontally;

  bool ltr = !base::i18n::IsRTL();
  bool on_left = (ltr && !flip_horizontally) || (!ltr && flip_horizontally);

  gtk_alignment_set_padding(GTK_ALIGNMENT(padding_),
      kInternalTopBottomPadding, kInternalTopBottomPadding,
      kInternalLeftRightPadding + (on_left ? 0 : kCornerSize),
      kInternalLeftRightPadding + (on_left ? kCornerSize : 0));
  // The rounded window code flips these arguments if we're RTL.
  gtk_util::SetRoundedWindowEdgesAndBorders(
      container_.get(),
      kCornerSize,
      flip_horizontally ?
          gtk_util::ROUNDED_TOP_LEFT :
          gtk_util::ROUNDED_TOP_RIGHT,
      gtk_util::BORDER_TOP |
          (flip_horizontally ? gtk_util::BORDER_LEFT : gtk_util::BORDER_RIGHT));
  gtk_widget_queue_draw(container_.get());
}

void StatusBubbleGtk::ExpandURL() {
  start_width_ = label_.get()->allocation.width;
  expand_animation_.reset(new ui::SlideAnimation(this));
  expand_animation_->SetTweenType(ui::Tween::LINEAR);
  expand_animation_->Show();

  SetStatusTextToURL();
}

void StatusBubbleGtk::UpdateLabelSizeRequest() {
  if (!expanded() || !expand_animation_->is_animating()) {
    gtk_widget_set_size_request(label_.get(), -1, -1);
    return;
  }

  int new_width = start_width_ +
      (desired_width_ - start_width_) * expand_animation_->GetCurrentValue();
  gtk_widget_set_size_request(label_.get(), new_width, -1);
}

// See http://crbug.com/68897 for why we have to handle this event.
gboolean StatusBubbleGtk::HandleEnterNotify(GtkWidget* sender,
                                            GdkEventCrossing* event) {
  ignore_next_left_content_ = true;
  MouseMoved(gfx::Point(event->x_root, event->y_root), false);
  return FALSE;
}

gboolean StatusBubbleGtk::HandleMotionNotify(GtkWidget* sender,
                                             GdkEventMotion* event) {
  MouseMoved(gfx::Point(event->x_root, event->y_root), false);
  return FALSE;
}

void StatusBubbleGtk::AnimationEnded(const ui::Animation* animation) {
  UpdateLabelSizeRequest();
}

void StatusBubbleGtk::AnimationProgressed(const ui::Animation* animation) {
  UpdateLabelSizeRequest();
}
