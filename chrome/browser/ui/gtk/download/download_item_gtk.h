// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_ITEM_GTK_H_
#define CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_ITEM_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class DownloadShelfContextMenuGtk;
class DownloadShelfGtk;
class GtkThemeService;
class NineBox;

namespace gfx {
class Image;
}

namespace ui {
class SlideAnimation;
}

class DownloadItemGtk : public content::DownloadItem::Observer,
                        public ui::AnimationDelegate,
                        public content::NotificationObserver {
 public:
  // DownloadItemGtk takes ownership of |download_item_model|.
  DownloadItemGtk(DownloadShelfGtk* parent_shelf,
                  content::DownloadItem* download_item);

  // Destroys all widgets belonging to this DownloadItemGtk.
  virtual ~DownloadItemGtk();

  // content::DownloadItem::Observer implementation.
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadDestroyed(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE;

  // ui::AnimationDelegate implementation.
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Called when the icon manager has finished loading the icon. We take
  // ownership of |icon_bitmap|.
  void OnLoadSmallIconComplete(gfx::Image* image);
  void OnLoadLargeIconComplete(gfx::Image* image);

  // Returns the DownloadItem model object belonging to this item.
  content::DownloadItem* download() { return download_model_.download(); }

 private:
  friend class DownloadShelfContextMenuGtk;

  // Functions for controlling the progress animation.
  // Repaint the download progress.
  void UpdateDownloadProgress();

  // Starts a repeating timer for UpdateDownloadProgress.
  void StartDownloadProgress();

  // Stops the repeating timer.
  void StopDownloadProgress();

  // Ask the icon manager to asynchronously start loading the icon for the file.
  void LoadIcon();

  // Sets the tooltip on the download button.
  void UpdateTooltip();

  // Sets the name label to the correct color.
  void UpdateNameLabel();

  // Sets the text of |status_label_| with the correct color.
  void UpdateStatusLabel(const std::string& status_text);

  // Sets the components of the danger warning.
  void UpdateDangerWarning();

  // Sets the icon for the danger warning dialog.
  void UpdateDangerIcon();

  // Reenables the download button after it has been clicked.
  void ReenableHbox();

  static void InitNineBoxes();

  // Show popup context menu. If |button| is not NULL, show the menu dropping
  // down from |button|. Otherwise, show the menu where the user clicks.
  void ShowPopupMenu(GtkWidget* button, GdkEventButton* event);

  // Draws everything in GTK rendering mode.
  CHROMEGTK_CALLBACK_1(DownloadItemGtk, gboolean, OnHboxExpose,
                       GdkEventExpose*);

  // Used for the download item's body and menu button in chrome theme mode.
  CHROMEGTK_CALLBACK_1(DownloadItemGtk, gboolean, OnExpose, GdkEventExpose*);

  // Called when |body_| is clicked.
  CHROMEGTK_CALLBACK_0(DownloadItemGtk, void, OnClick);

  // Called when |body_| is pressed with mouse button. This function is used to
  // show popup menu with right button click.
  CHROMEGTK_CALLBACK_1(DownloadItemGtk, gboolean, OnButtonPress,
                       GdkEventButton*);

  // Used for the download icon.
  CHROMEGTK_CALLBACK_1(DownloadItemGtk, gboolean, OnProgressAreaExpose,
                       GdkEventExpose*);

  CHROMEGTK_CALLBACK_1(DownloadItemGtk, gboolean, OnMenuButtonPressEvent,
                       GdkEventButton*);

  // Dangerous download related. -----------------------------------------------
  CHROMEGTK_CALLBACK_1(DownloadItemGtk, gboolean, OnDangerousPromptExpose,
                       GdkEventExpose*);
  CHROMEGTK_CALLBACK_0(DownloadItemGtk, void, OnDangerousAccept);
  CHROMEGTK_CALLBACK_0(DownloadItemGtk, void, OnDangerousDecline);

  // Nineboxes for the body area.
  static NineBox* body_nine_box_normal_;
  static NineBox* body_nine_box_prelight_;
  static NineBox* body_nine_box_active_;

  // Nineboxes for the menu button.
  static NineBox* menu_nine_box_normal_;
  static NineBox* menu_nine_box_prelight_;
  static NineBox* menu_nine_box_active_;

  // Ninebox for the background of the dangerous download prompt.
  static NineBox* dangerous_nine_box_;

  // The shelf on which we are displayed.
  DownloadShelfGtk* parent_shelf_;

  // The widget that contains the body and menu dropdown.
  ui::OwnedWidgetGtk hbox_;

  // The widget that contains the name of the download and the progress
  // animation.
  ui::OwnedWidgetGtk body_;

  // The widget that contains the texts of |name_label_| and |status_label_|.
  GtkWidget* text_stack_;

  // The GtkLabel that holds the download title text.
  GtkWidget* name_label_;

  // The GtkLabel that holds the status text.
  GtkWidget* status_label_;

  // The current text of status label
  std::string status_text_;

  // The widget that creates a dropdown menu when pressed.
  GtkWidget* menu_button_;

  // A gtk arrow pointing downward displayed in |menu_button_|. Only displayed
  // in GTK mode.
  GtkWidget* arrow_;

  // Whether the menu is currently showing for |menu_button_|. Affects how we
  // draw the button.
  bool menu_showing_;

  // Whether we should use the GTK text color
  GtkThemeService* theme_service_;

  // The widget that contains the animation progress and the file's icon
  // (as well as the complete animation).
  ui::OwnedWidgetGtk progress_area_;

  // In degrees. Only used for downloads with no known total size.
  int progress_angle_;

  // The menu that pops down when the user presses |menu_button_|. We do not
  // create this until the first time we actually need it.
  scoped_ptr<DownloadShelfContextMenuGtk> menu_;

  // The download item model we represent.
  DownloadItemModel download_model_;

  // The dangerous download dialog. This will be null for safe downloads.
  GtkWidget* dangerous_prompt_;
  GtkWidget* dangerous_image_;
  GtkWidget* dangerous_label_;

  // An hbox for holding components of the dangerous download dialog.
  ui::OwnedWidgetGtk dangerous_hbox_;
  int dangerous_hbox_start_width_;
  int dangerous_hbox_full_width_;

  // The animation when this item is first added to the shelf.
  scoped_ptr<ui::SlideAnimation> new_item_animation_;

  // Progress animation.
  base::RepeatingTimer<DownloadItemGtk> progress_timer_;

  // Animation for download complete.
  ui::SlideAnimation complete_animation_;

  // The file icon for the download. May be null. The small version is used
  // for display in the shelf; the large version is for use as a drag icon.
  // These icons are owned by the IconManager (owned by the BrowserProcess).
  gfx::Image* icon_small_;
  gfx::Image* icon_large_;

  // The last download file path for which we requested an icon.
  FilePath icon_filepath_;

  content::NotificationRegistrar registrar_;

  // The time at which we were insantiated.
  base::Time creation_time_;

  // For canceling an in progress icon request.
  CancelableTaskTracker cancelable_task_tracker_;

  // Indicates when the download has completed, so we don't redo
  // on-completion actions.
  bool download_complete_;

  // Whether we are currently disabled as part of opening the downloaded file.
  bool disabled_while_opening_;

  // Method factory used to delay reenabling of the item when opening the
  // downloaded file.
  base::WeakPtrFactory<DownloadItemGtk> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_ITEM_GTK_H_
