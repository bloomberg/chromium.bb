// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/download/download_item_gtk.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/custom_drag.h"
#include "chrome/browser/ui/gtk/download/download_shelf_context_menu_gtk.h"
#include "chrome/browser/ui/gtk/download/download_shelf_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/nine_box.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_utils_gtk.h"

namespace {

// The width of the |menu_button_| widget. It has to be at least as wide as the
// bitmap that we use to draw it, i.e. 16, but can be more.
const int kMenuButtonWidth = 16;

// Padding on left and right of items in dangerous download prompt.
const int kDangerousElementPadding = 3;

// Minimum width of the dangerous download message at which we will start
// wrapping.
const int kDangerousTextWidth = 200;

// Amount of space we allot to showing the filename. If the filename is too wide
// it will be elided.
const int kTextWidth = 140;

// We only cap the size of the tooltip so we don't crash.
const int kTooltipMaxWidth = 1000;

// The minimum width we will ever draw the download item. Used as a lower bound
// during animation. This number comes from the width of the images used to
// make the download item.
const int kMinDownloadItemWidth = download_util::kSmallProgressIconSize;

// New download item animation speed in milliseconds.
const int kNewItemAnimationDurationMs = 800;

// How long the 'download complete/interrupted' animation should last for.
const int kCompleteAnimationDurationMs = 2500;

// Height of the body.
const int kBodyHeight = download_util::kSmallProgressIconSize;

// Width of the body area of the download item.
// TODO(estade): get rid of the fudge factor. http://crbug.com/18692
const int kBodyWidth = kTextWidth + 50 + download_util::kSmallProgressIconSize;

// The font size of the text, and that size rounded down to the nearest integer
// for the size of the arrow in GTK theme mode.
const double kTextSize = 13.4;  // 13.4px == 10pt @ 96dpi

// Darken light-on-dark download status text by 20% before drawing, thus
// creating a "muted" version of title text for both dark-on-light and
// light-on-dark themes.
static const double kDownloadItemLuminanceMod = 0.8;

// How long we keep the item disabled after the user clicked it to open the
// downloaded item.
static const int kDisabledOnOpenDurationMs = 3000;

}  // namespace

NineBox* DownloadItemGtk::body_nine_box_normal_ = NULL;
NineBox* DownloadItemGtk::body_nine_box_prelight_ = NULL;
NineBox* DownloadItemGtk::body_nine_box_active_ = NULL;

NineBox* DownloadItemGtk::menu_nine_box_normal_ = NULL;
NineBox* DownloadItemGtk::menu_nine_box_prelight_ = NULL;
NineBox* DownloadItemGtk::menu_nine_box_active_ = NULL;

NineBox* DownloadItemGtk::dangerous_nine_box_ = NULL;

using content::DownloadItem;

DownloadItemGtk::DownloadItemGtk(DownloadShelfGtk* parent_shelf,
                                 DownloadItem* download_item)
    : parent_shelf_(parent_shelf),
      arrow_(NULL),
      menu_showing_(false),
      theme_service_(
          GtkThemeService::GetFrom(parent_shelf->browser()->profile())),
      progress_angle_(download_util::kStartAngleDegrees),
      download_model_(download_item),
      dangerous_prompt_(NULL),
      dangerous_label_(NULL),
      complete_animation_(this),
      icon_small_(NULL),
      icon_large_(NULL),
      creation_time_(base::Time::Now()),
      download_complete_(false),
      disabled_while_opening_(false),
      weak_ptr_factory_(this) {
  LoadIcon();

  body_.Own(gtk_button_new());
  gtk_widget_set_app_paintable(body_.get(), TRUE);
  UpdateTooltip();

  g_signal_connect(body_.get(), "expose-event",
                   G_CALLBACK(OnExposeThunk), this);
  g_signal_connect(body_.get(), "clicked",
                   G_CALLBACK(OnClickThunk), this);
  g_signal_connect(body_.get(), "button-press-event",
                   G_CALLBACK(OnButtonPressThunk), this);
  gtk_widget_set_can_focus(body_.get(), FALSE);
  // Remove internal padding on the button.
  GtkRcStyle* no_padding_style = gtk_rc_style_new();
  no_padding_style->xthickness = 0;
  no_padding_style->ythickness = 0;
  gtk_widget_modify_style(body_.get(), no_padding_style);
  g_object_unref(no_padding_style);

  name_label_ = gtk_label_new(NULL);
  // Left align and vertically center the labels.
  gtk_misc_set_alignment(GTK_MISC(name_label_), 0, 0.5);
  // Until we switch to vector graphics, force the font size.
  gtk_util::ForceFontSizePixels(name_label_, kTextSize);

  UpdateNameLabel();

  status_label_ = NULL;

  // Stack the labels on top of one another.
  text_stack_ = gtk_vbox_new(FALSE, 0);
  g_signal_connect(text_stack_, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &text_stack_);
  gtk_box_pack_start(GTK_BOX(text_stack_), name_label_, TRUE, TRUE, 0);

  // We use a GtkFixed because we don't want it to have its own window.
  // This choice of widget is not critically important though.
  progress_area_.Own(gtk_fixed_new());
  gtk_widget_set_size_request(progress_area_.get(),
      download_util::kSmallProgressIconSize,
      download_util::kSmallProgressIconSize);
  gtk_widget_set_app_paintable(progress_area_.get(), TRUE);
  g_signal_connect(progress_area_.get(), "expose-event",
                   G_CALLBACK(OnProgressAreaExposeThunk), this);

  // Put the download progress icon on the left of the labels.
  GtkWidget* body_hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(body_.get()), body_hbox);
  gtk_box_pack_start(GTK_BOX(body_hbox), progress_area_.get(), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(body_hbox), text_stack_, TRUE, TRUE, 0);

  menu_button_ = gtk_button_new();
  gtk_widget_set_app_paintable(menu_button_, TRUE);
  gtk_widget_set_can_focus(menu_button_, FALSE);
  g_signal_connect(menu_button_, "expose-event",
                   G_CALLBACK(OnExposeThunk), this);
  g_signal_connect(menu_button_, "button-press-event",
                   G_CALLBACK(OnMenuButtonPressEventThunk), this);
  g_object_set_data(G_OBJECT(menu_button_), "left-align-popup",
                    reinterpret_cast<void*>(true));

  GtkWidget* shelf_hbox = parent_shelf->GetHBox();
  hbox_.Own(gtk_hbox_new(FALSE, 0));
  g_signal_connect(hbox_.get(), "expose-event",
                   G_CALLBACK(OnHboxExposeThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox_.get()), body_.get(), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_.get()), menu_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(shelf_hbox), hbox_.get(), FALSE, FALSE, 0);
  // Insert as the leftmost item.
  gtk_box_reorder_child(GTK_BOX(shelf_hbox), hbox_.get(), 0);

  download()->AddObserver(this);

  new_item_animation_.reset(new ui::SlideAnimation(this));
  new_item_animation_->SetSlideDuration(kNewItemAnimationDurationMs);
  gtk_widget_show_all(hbox_.get());

  if (download_model_.IsDangerous()) {
    // Hide the download item components for now.
    gtk_widget_set_no_show_all(body_.get(), TRUE);
    gtk_widget_set_no_show_all(menu_button_, TRUE);
    gtk_widget_hide(body_.get());
    gtk_widget_hide(menu_button_);

    // Create an hbox to hold it all.
    dangerous_hbox_.Own(gtk_hbox_new(FALSE, kDangerousElementPadding));

    // Add padding at the beginning and end. The hbox will add padding between
    // the empty labels and the other elements.
    GtkWidget* empty_label_a = gtk_label_new(NULL);
    GtkWidget* empty_label_b = gtk_label_new(NULL);
    gtk_box_pack_start(GTK_BOX(dangerous_hbox_.get()), empty_label_a,
                       FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(dangerous_hbox_.get()), empty_label_b,
                     FALSE, FALSE, 0);

    // Create the warning icon.
    dangerous_image_ = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(dangerous_hbox_.get()), dangerous_image_,
                       FALSE, FALSE, 0);

    dangerous_label_ = gtk_label_new(NULL);
    // We pass TRUE, TRUE so that the label will condense to less than its
    // request when the animation is going on.
    gtk_box_pack_start(GTK_BOX(dangerous_hbox_.get()), dangerous_label_,
                       TRUE, TRUE, 0);

    // Create the nevermind button.
    GtkWidget* dangerous_decline = gtk_button_new_with_label(
        l10n_util::GetStringUTF8(IDS_DISCARD_DOWNLOAD).c_str());
    g_signal_connect(dangerous_decline, "clicked",
                     G_CALLBACK(OnDangerousDeclineThunk), this);
    gtk_util::CenterWidgetInHBox(dangerous_hbox_.get(), dangerous_decline,
                                 false, 0);

    // Create the ok button.
    GtkWidget* dangerous_accept = gtk_button_new_with_label(
        UTF16ToUTF8(download_model_.GetWarningConfirmButtonText()).c_str());
    g_signal_connect(dangerous_accept, "clicked",
                     G_CALLBACK(OnDangerousAcceptThunk), this);
    gtk_util::CenterWidgetInHBox(dangerous_hbox_.get(), dangerous_accept, false,
                                 0);

    // Put it in an alignment so that padding will be added on the left and
    // right.
    dangerous_prompt_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(dangerous_prompt_),
        0, 0, kDangerousElementPadding, kDangerousElementPadding);
    gtk_container_add(GTK_CONTAINER(dangerous_prompt_), dangerous_hbox_.get());
    gtk_box_pack_start(GTK_BOX(hbox_.get()), dangerous_prompt_, FALSE, FALSE,
                       0);
    gtk_widget_set_app_paintable(dangerous_prompt_, TRUE);
    gtk_widget_set_redraw_on_allocate(dangerous_prompt_, TRUE);
    g_signal_connect(dangerous_prompt_, "expose-event",
                     G_CALLBACK(OnDangerousPromptExposeThunk), this);
    gtk_widget_show_all(dangerous_prompt_);
  }

  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
  theme_service_->InitThemesFor(this);

  // Set the initial width of the widget to be animated.
  if (download_model_.IsDangerous()) {
    gtk_widget_set_size_request(dangerous_hbox_.get(),
                                dangerous_hbox_start_width_, -1);
  } else {
    gtk_widget_set_size_request(body_.get(), kMinDownloadItemWidth, -1);
  }

  new_item_animation_->Show();

  complete_animation_.SetTweenType(ui::Tween::LINEAR);
  complete_animation_.SetSlideDuration(kCompleteAnimationDurationMs);

  // Update the status text and animation state.
  OnDownloadUpdated(download());
}

DownloadItemGtk::~DownloadItemGtk() {
  // First close the menu and then destroy the GtkWidgets. Bug#97724
  if (menu_.get())
    menu_.reset();

  StopDownloadProgress();
  download()->RemoveObserver(this);

  // We may free some shelf space for showing more download items.
  parent_shelf_->MaybeShowMoreDownloadItems();

  hbox_.Destroy();
  progress_area_.Destroy();
  body_.Destroy();
  dangerous_hbox_.Destroy();

  // Make sure this widget has been destroyed and the pointer we hold to it
  // NULLed.
  DCHECK(!status_label_);
}

void DownloadItemGtk::OnDownloadUpdated(DownloadItem* download_item) {
  DCHECK_EQ(download(), download_item);

  if (dangerous_prompt_ != NULL && !download_model_.IsDangerous()) {
    // We have been approved.
    gtk_widget_set_no_show_all(body_.get(), FALSE);
    gtk_widget_set_no_show_all(menu_button_, FALSE);
    gtk_widget_show_all(hbox_.get());
    gtk_widget_destroy(dangerous_prompt_);
    gtk_widget_set_size_request(body_.get(), kBodyWidth, -1);
    dangerous_prompt_ = NULL;

    // We may free some shelf space for showing more download items.
    parent_shelf_->MaybeShowMoreDownloadItems();
  }

  if (download()->GetUserVerifiedFilePath() != icon_filepath_) {
    // Turns out the file path is "Unconfirmed %d.crdownload" for dangerous
    // downloads. When the download is confirmed, the file is renamed on
    // another thread, so reload the icon if the download filename changes.
    LoadIcon();

    UpdateTooltip();
  }

  switch (download()->GetState()) {
    case DownloadItem::CANCELLED:
      StopDownloadProgress();
      gtk_widget_queue_draw(progress_area_.get());
      break;
    case DownloadItem::INTERRUPTED:
      StopDownloadProgress();
      UpdateTooltip();

      complete_animation_.Show();
      break;
    case DownloadItem::COMPLETE:
      // GetAutoOpened() may change after the download's initial transition to
      // COMPLETE, so we check it before the idemopotency shield below.
      if (download()->GetAutoOpened()) {
        parent_shelf_->RemoveDownloadItem(this);  // This will delete us!
        return;
      }

      // We've already handled the completion specific actions; skip
      // doing the non-idempotent ones again.
      if (download_complete_)
        break;

      StopDownloadProgress();

      // Set up the widget as a drag source.
      DownloadItemDrag::SetSource(body_.get(), download(), icon_large_);

      complete_animation_.Show();
      download_complete_ = true;
      break;
    case DownloadItem::IN_PROGRESS:
      download()->IsPaused() ?
          StopDownloadProgress() : StartDownloadProgress();
      break;
    default:
      NOTREACHED();
  }

  status_text_ = UTF16ToUTF8(download_model_.GetStatusText());
  UpdateStatusLabel(status_text_);
}

void DownloadItemGtk::OnDownloadDestroyed(DownloadItem* download_item) {
  DCHECK_EQ(download(), download_item);
  parent_shelf_->RemoveDownloadItem(this);
  // This will delete us!
}

void DownloadItemGtk::AnimationProgressed(const ui::Animation* animation) {
  if (animation == &complete_animation_) {
    gtk_widget_queue_draw(progress_area_.get());
  } else {
    DCHECK(animation == new_item_animation_.get());
    if (download_model_.IsDangerous()) {
      int progress = static_cast<int>((dangerous_hbox_full_width_ -
                                       dangerous_hbox_start_width_) *
                                      animation->GetCurrentValue());
      int showing_width = dangerous_hbox_start_width_ + progress;
      gtk_widget_set_size_request(dangerous_hbox_.get(), showing_width, -1);
    } else {
      int showing_width = std::max(kMinDownloadItemWidth,
          static_cast<int>(kBodyWidth * animation->GetCurrentValue()));
      gtk_widget_set_size_request(body_.get(), showing_width, -1);
    }
  }
}

void DownloadItemGtk::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_BROWSER_THEME_CHANGED) {
    // Our GtkArrow is only visible in gtk mode. Otherwise, we let the custom
    // rendering code do whatever it wants.
    if (theme_service_->UsingNativeTheme()) {
      if (!arrow_) {
        arrow_ = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
        gtk_widget_set_size_request(arrow_,
                                    static_cast<int>(kTextSize),
                                    static_cast<int>(kTextSize));
        gtk_container_add(GTK_CONTAINER(menu_button_), arrow_);
      }

      gtk_widget_set_size_request(menu_button_, -1, -1);
      gtk_widget_show(arrow_);
    } else {
      InitNineBoxes();

      gtk_widget_set_size_request(menu_button_, kMenuButtonWidth, 0);

      if (arrow_)
        gtk_widget_hide(arrow_);
    }

    UpdateNameLabel();
    UpdateStatusLabel(status_text_);
    UpdateDangerWarning();
  }
}

// Download progress animation functions.

void DownloadItemGtk::UpdateDownloadProgress() {
  progress_angle_ = (progress_angle_ +
                     download_util::kUnknownIncrementDegrees) %
                    download_util::kMaxDegrees;
  gtk_widget_queue_draw(progress_area_.get());
}

void DownloadItemGtk::StartDownloadProgress() {
  if (progress_timer_.IsRunning())
    return;
  progress_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(download_util::kProgressRateMs), this,
      &DownloadItemGtk::UpdateDownloadProgress);
}

void DownloadItemGtk::StopDownloadProgress() {
  progress_timer_.Stop();
}

// Icon loading functions.

void DownloadItemGtk::OnLoadSmallIconComplete(gfx::Image* image) {
  icon_small_ = image;
  gtk_widget_queue_draw(progress_area_.get());
}

void DownloadItemGtk::OnLoadLargeIconComplete(gfx::Image* image) {
  icon_large_ = image;
  DownloadItemDrag::SetSource(body_.get(), download(), icon_large_);
}

void DownloadItemGtk::LoadIcon() {
  cancelable_task_tracker_.TryCancelAll();
  IconManager* im = g_browser_process->icon_manager();
  icon_filepath_ = download()->GetUserVerifiedFilePath();
  im->LoadIcon(icon_filepath_,
               IconLoader::SMALL,
               base::Bind(&DownloadItemGtk::OnLoadSmallIconComplete,
                          base::Unretained(this)),
               &cancelable_task_tracker_);
  im->LoadIcon(icon_filepath_,
               IconLoader::LARGE,
               base::Bind(&DownloadItemGtk::OnLoadLargeIconComplete,
                          base::Unretained(this)),
               &cancelable_task_tracker_);
}

void DownloadItemGtk::UpdateTooltip() {
  string16 tooltip_text =
      download_model_.GetTooltipText(gfx::Font(), kTooltipMaxWidth);
  gtk_widget_set_tooltip_text(body_.get(), UTF16ToUTF8(tooltip_text).c_str());
}

void DownloadItemGtk::UpdateNameLabel() {
  // TODO(estade): This is at best an educated guess, since we don't actually
  // use gfx::Font() to draw the text. This is why we need to add so
  // much padding when we set the size request. We need to either use gfx::Font
  // or somehow extend TextElider.
  gfx::Font font = gfx::Font();
  string16 filename;
  if (!disabled_while_opening_) {
    filename = ui::ElideFilename(
        download()->GetFileNameToReportUser(), font, kTextWidth);
  } else {
    // First, Calculate the download status opening string width.
    string16 status_string =
        l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_OPENING, string16());
    int status_string_width = font.GetStringWidth(status_string);
    // Then, elide the file name.
    string16 filename_string =
        ui::ElideFilename(download()->GetFileNameToReportUser(), font,
                          kTextWidth - status_string_width);
    // Last, concat the whole string.
    filename = l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_OPENING,
                                                 filename_string);
  }

  GdkColor color = theme_service_->GetGdkColor(
      ThemeService::COLOR_BOOKMARK_TEXT);
  gtk_util::SetLabelColor(
      name_label_,
      theme_service_->UsingNativeTheme() ? NULL : &color);
  gtk_label_set_text(GTK_LABEL(name_label_),
                     UTF16ToUTF8(filename).c_str());
}

void DownloadItemGtk::UpdateStatusLabel(const std::string& status_text) {
  if (!text_stack_) {
    // At least our container has been destroyed, which means that
    // this item is on the way to being destroyed; don't do anything.
    return;
  }

  // If |status_text| is empty, only |name_label_| is displayed at the
  // vertical center of |text_stack_|. Otherwise, |name_label_| is displayed
  // on the upper half of |text_stack_| and |status_label_| is displayed
  // on the lower half of |text_stack_|.
  if (status_text.empty()) {
    if (status_label_)
      gtk_widget_destroy(status_label_);
    return;
  }
  if (!status_label_) {
    status_label_ = gtk_label_new(NULL);
    g_signal_connect(status_label_, "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &status_label_);
    // Left align and vertically center the labels.
    gtk_misc_set_alignment(GTK_MISC(status_label_), 0, 0.5);
    // Until we switch to vector graphics, force the font size.
    gtk_util::ForceFontSizePixels(status_label_, kTextSize);

    gtk_box_pack_start(GTK_BOX(text_stack_), status_label_, FALSE, FALSE, 0);
    gtk_widget_show_all(status_label_);
  }

  GdkColor text_color;
  if (!theme_service_->UsingNativeTheme()) {
    SkColor color = theme_service_->GetColor(
        ThemeService::COLOR_BOOKMARK_TEXT);
    if (color_utils::RelativeLuminance(color) > 0.5) {
      color = SkColorSetRGB(
          static_cast<int>(kDownloadItemLuminanceMod *
                           SkColorGetR(color)),
          static_cast<int>(kDownloadItemLuminanceMod *
                           SkColorGetG(color)),
          static_cast<int>(kDownloadItemLuminanceMod *
                           SkColorGetB(color)));
    }

    // Lighten the color by blending it with the download item body color. These
    // values are taken from IDR_DOWNLOAD_BUTTON.
    SkColor blend_color = SkColorSetRGB(241, 245, 250);
    text_color = gfx::SkColorToGdkColor(
        color_utils::AlphaBlend(blend_color, color, 77));
  }

  gtk_util::SetLabelColor(
      status_label_,
      theme_service_->UsingNativeTheme() ? NULL : &text_color);
  gtk_label_set_text(GTK_LABEL(status_label_), status_text.c_str());
}

void DownloadItemGtk::UpdateDangerWarning() {
  if (dangerous_prompt_) {
    UpdateDangerIcon();

    // We create |dangerous_warning| as a wide string so we can more easily
    // calculate its length in characters.
    string16 dangerous_warning =
        download_model_.GetWarningText(gfx::Font(), kTextWidth);
    if (theme_service_->UsingNativeTheme()) {
      gtk_util::SetLabelColor(dangerous_label_, NULL);
    } else {
      GdkColor color = theme_service_->GetGdkColor(
          ThemeService::COLOR_BOOKMARK_TEXT);
      gtk_util::SetLabelColor(dangerous_label_, &color);
    }

    gtk_label_set_text(GTK_LABEL(dangerous_label_),
                       UTF16ToUTF8(dangerous_warning).c_str());

    // Until we switch to vector graphics, force the font size.
    gtk_util::ForceFontSizePixels(dangerous_label_, kTextSize);

    gtk_widget_set_size_request(dangerous_label_, -1, -1);
    gtk_label_set_line_wrap(GTK_LABEL(dangerous_label_), FALSE);

    GtkRequisition req;
    gtk_widget_size_request(dangerous_label_, &req);

    gint label_width = req.width;
    if (req.width > kDangerousTextWidth) {
      // If the label width exceeds kDangerousTextWidth, we try line wrapping
      // starting at 60% and increasing in 10% intervals of the full width until
      // we have a label that fits within the height constraints of the shelf.
      gtk_label_set_line_wrap(GTK_LABEL(dangerous_label_), TRUE);
      int full_width = req.width;
      int tenths = 6;
      do {
        label_width = full_width * tenths / 10;
        gtk_widget_set_size_request(dangerous_label_, label_width, -1);
        gtk_widget_size_request(dangerous_label_, &req);
      } while (req.height > kBodyHeight && ++tenths <= 10);
      DCHECK(req.height <= kBodyHeight);
    }

    // The width will depend on the text. We must do this each time we possibly
    // change the label above.
    gtk_widget_size_request(dangerous_hbox_.get(), &req);
    dangerous_hbox_full_width_ = req.width;
    dangerous_hbox_start_width_ = dangerous_hbox_full_width_ - label_width;
  }
}

void DownloadItemGtk::UpdateDangerIcon() {
  if (theme_service_->UsingNativeTheme()) {
    const char* stock = download_model_.IsMalicious() ?
        GTK_STOCK_DIALOG_ERROR : GTK_STOCK_DIALOG_WARNING;
    gtk_image_set_from_stock(
        GTK_IMAGE(dangerous_image_), stock, GTK_ICON_SIZE_SMALL_TOOLBAR);
  } else {
    // Set the warning icon.
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    int pixbuf_id = download_model_.IsMalicious() ? IDR_SAFEBROWSING_WARNING
                                                  : IDR_WARNING;
    gtk_image_set_from_pixbuf(GTK_IMAGE(dangerous_image_),
                              rb.GetNativeImageNamed(pixbuf_id).ToGdkPixbuf());
  }
}

// static
void DownloadItemGtk::InitNineBoxes() {
  if (body_nine_box_normal_)
    return;

  body_nine_box_normal_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM);

  body_nine_box_prelight_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP_H,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP_H,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP_H,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_H,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_H,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_H,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_H,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_H,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_H);

  body_nine_box_active_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP_P,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP_P,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP_P,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_P,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_P,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_P,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_P,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_P,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_P);

  menu_nine_box_normal_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_MENU_TOP, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_MIDDLE, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_BOTTOM, 0, 0);

  menu_nine_box_prelight_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_MENU_TOP_H, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_H, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_H, 0, 0);

  menu_nine_box_active_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_MENU_TOP_P, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_P, 0, 0,
      IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_P, 0, 0);

  dangerous_nine_box_ = new NineBox(
      IDR_DOWNLOAD_BUTTON_LEFT_TOP,
      IDR_DOWNLOAD_BUTTON_CENTER_TOP,
      IDR_DOWNLOAD_BUTTON_RIGHT_TOP_NO_DD,
      IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE,
      IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE,
      IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_NO_DD,
      IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM,
      IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM,
      IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_NO_DD);
}

gboolean DownloadItemGtk::OnHboxExpose(GtkWidget* widget, GdkEventExpose* e) {
  TRACE_EVENT0("ui::gtk", "DownloadItemGtk::OnHboxExpose");
  if (theme_service_->UsingNativeTheme()) {
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    int border_width = gtk_container_get_border_width(GTK_CONTAINER(widget));
    int x = allocation.x + border_width;
    int y = allocation.y + border_width;
    int width = allocation.width - border_width * 2;
    int height = allocation.height - border_width * 2;

    if (download_model_.IsDangerous()) {
      // Draw a simple frame around the area when we're displaying the warning.
      gtk_paint_shadow(gtk_widget_get_style(widget),
                       gtk_widget_get_window(widget),
                       gtk_widget_get_state(widget),
                       static_cast<GtkShadowType>(GTK_SHADOW_OUT),
                       &e->area, widget, "frame",
                       x, y, width, height);
    } else {
      // Manually draw the GTK button border around the download item. We draw
      // the left part of the button (the file), a divider, and then the right
      // part of the button (the menu). We can't draw a button on top of each
      // other (*cough*Clearlooks*cough*) so instead, to draw the left part of
      // the button, we instruct GTK to draw the entire button...with a
      // doctored clip rectangle to the left part of the button sans
      // separator. We then repeat this for the right button.
      GtkStyle* style = gtk_widget_get_style(body_.get());

      GtkAllocation left_clip;
      gtk_widget_get_allocation(body_.get(), &left_clip);

      GtkAllocation right_clip;
      gtk_widget_get_allocation(menu_button_, &right_clip);

      GtkShadowType body_shadow =
          GTK_BUTTON(body_.get())->depressed ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
      gtk_paint_box(style,
                    gtk_widget_get_window(widget),
                    gtk_widget_get_state(body_.get()),
                    body_shadow,
                    &left_clip, widget, "button",
                    x, y, width, height);

      GtkShadowType menu_shadow =
          GTK_BUTTON(menu_button_)->depressed ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
      gtk_paint_box(style,
                    gtk_widget_get_window(widget),
                    gtk_widget_get_state(menu_button_),
                    menu_shadow,
                    &right_clip, widget, "button",
                    x, y, width, height);

      // Doing the math to reverse engineer where we should be drawing our line
      // is hard and relies on copying GTK internals, so instead steal the
      // allocation of the gtk arrow which is close enough (and will error on
      // the conservative side).
      GtkAllocation arrow_allocation;
      gtk_widget_get_allocation(arrow_, &arrow_allocation);
      gtk_paint_vline(style,
                      gtk_widget_get_window(widget),
                      gtk_widget_get_state(widget),
                      &e->area, widget, "button",
                      arrow_allocation.y,
                      arrow_allocation.y + arrow_allocation.height,
                      left_clip.x + left_clip.width);
    }
  }
  return FALSE;
}

gboolean DownloadItemGtk::OnExpose(GtkWidget* widget, GdkEventExpose* e) {
  TRACE_EVENT0("ui::gtk", "DownloadItemGtk::OnExpose");
  if (!theme_service_->UsingNativeTheme()) {
    bool is_body = widget == body_.get();

    NineBox* nine_box = NULL;
    // If true, this widget is |body_|, otherwise it is |menu_button_|.
    if (gtk_widget_get_state(widget) == GTK_STATE_PRELIGHT)
      nine_box = is_body ? body_nine_box_prelight_ : menu_nine_box_prelight_;
    else if (gtk_widget_get_state(widget) == GTK_STATE_ACTIVE)
      nine_box = is_body ? body_nine_box_active_ : menu_nine_box_active_;
    else
      nine_box = is_body ? body_nine_box_normal_ : menu_nine_box_normal_;

    // When the button is showing, we want to draw it as active. We have to do
    // this explicitly because the button's state will be NORMAL while the menu
    // has focus.
    if (!is_body && menu_showing_)
      nine_box = menu_nine_box_active_;

    nine_box->RenderToWidget(widget);
  }

  GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
  if (child)
    gtk_container_propagate_expose(GTK_CONTAINER(widget), child, e);

  return TRUE;
}

void DownloadItemGtk::ReenableHbox() {
  gtk_widget_set_sensitive(hbox_.get(), true);
  disabled_while_opening_ = false;
  UpdateNameLabel();
}

void DownloadItemGtk::OnDownloadOpened(DownloadItem* download) {
  disabled_while_opening_ = true;
  gtk_widget_set_sensitive(hbox_.get(), false);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DownloadItemGtk::ReenableHbox,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kDisabledOnOpenDurationMs));
  UpdateNameLabel();
  parent_shelf_->ItemOpened();
}

void DownloadItemGtk::OnClick(GtkWidget* widget) {
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.open_download",
                           base::Time::Now() - creation_time_);
  download()->OpenDownload();
}

gboolean DownloadItemGtk::OnButtonPress(GtkWidget* button,
                                        GdkEventButton* event) {
  if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
    ShowPopupMenu(NULL, event);
    return TRUE;
  }
  return FALSE;
}

gboolean DownloadItemGtk::OnProgressAreaExpose(GtkWidget* widget,
                                               GdkEventExpose* event) {
  TRACE_EVENT0("ui::gtk", "DownloadItemGtk::OnProgressAreaExpose");

  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);

  // Create a transparent canvas.
  gfx::CanvasSkiaPaint canvas(event, false);
  if (complete_animation_.is_animating()) {
    if (download()->IsInterrupted()) {
      download_util::PaintDownloadInterrupted(&canvas,
          allocation.x, allocation.y,
          complete_animation_.GetCurrentValue(),
          download_util::SMALL);
    } else {
      download_util::PaintDownloadComplete(&canvas,
          allocation.x, allocation.y,
          complete_animation_.GetCurrentValue(),
          download_util::SMALL);
    }
  } else if (download()->IsInProgress()) {
    download_util::PaintDownloadProgress(&canvas,
        allocation.x, allocation.y,
        progress_angle_,
        download_model_.PercentComplete(),
        download_util::SMALL);
  }

  // |icon_small_| may be NULL if it is still loading. If the file is an
  // unrecognized type then we will get back a generic system icon. Hence
  // there is no need to use the chromium-specific default download item icon.
  if (icon_small_) {
    const int offset = download_util::kSmallProgressIconOffset;
    canvas.DrawImageInt(icon_small_->AsImageSkia(),
        allocation.x + offset, allocation.y + offset);
  }

  return TRUE;
}

gboolean DownloadItemGtk::OnMenuButtonPressEvent(GtkWidget* button,
                                                 GdkEventButton* event) {
  if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
    ShowPopupMenu(button, event);
    menu_showing_ = true;
    gtk_widget_queue_draw(button);
    return TRUE;
  }
  return FALSE;
}

void DownloadItemGtk::ShowPopupMenu(GtkWidget* button,
                                    GdkEventButton* event) {
  // Stop any completion animation.
  if (complete_animation_.is_animating())
    complete_animation_.End();

  if (!menu_.get()) {
    menu_.reset(new DownloadShelfContextMenuGtk(this,
                                                parent_shelf_->GetNavigator()));
  }
  menu_->Popup(button, event);
}

gboolean DownloadItemGtk::OnDangerousPromptExpose(GtkWidget* widget,
                                                  GdkEventExpose* event) {
  TRACE_EVENT0("ui::gtk", "DownloadItemGtk::OnDangerousPromptExpose");
  if (!theme_service_->UsingNativeTheme()) {
    // The hbox renderer will take care of the border when in GTK mode.
    dangerous_nine_box_->RenderToWidget(widget);
  }
  return FALSE;  // Continue propagation.
}

void DownloadItemGtk::OnDangerousAccept(GtkWidget* button) {
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.save_download",
                           base::Time::Now() - creation_time_);
  download()->DangerousDownloadValidated();
}

void DownloadItemGtk::OnDangerousDecline(GtkWidget* button) {
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.discard_download",
                           base::Time::Now() - creation_time_);
  if (download()->IsPartialDownload())
    download()->Cancel(true);
  download()->Delete(DownloadItem::DELETE_DUE_TO_USER_DISCARD);
}
