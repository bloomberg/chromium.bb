// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_VIEWS_H_

#include <map>

#include "base/compiler_specific.h"
#include "chrome/browser/media_galleries/media_galleries_scan_result_dialog_controller.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class ImageButton;
class Label;
class MenuRunner;
class Widget;
}

// The media galleries scan result view for Views. It will immediately show
// upon construction.
class MediaGalleriesScanResultDialogViews
    : public MediaGalleriesScanResultDialog,
      public views::ButtonListener,
      public views::ContextMenuController,
      public views::DialogDelegate {
 public:
  explicit MediaGalleriesScanResultDialogViews(
      MediaGalleriesScanResultDialogController* controller);
  virtual ~MediaGalleriesScanResultDialogViews();

  // MediaGalleriesScanResultDialog implementation:
  virtual void UpdateResults() OVERRIDE;

  // views::DialogDelegate implementation:
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::ContextMenuController implementation:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& point,
                                      ui::MenuSourceType source_type) OVERRIDE;

 private:
  struct GalleryEntry {
    views::Checkbox* checkbox;
    views::ImageButton* folder_viewer_button;
    views::Label* secondary_text;
  };
  typedef std::map<MediaGalleryPrefId, GalleryEntry> GalleryViewMap;

  void InitChildViews();

  // Adds a checkbox or updates an existing checkbox. Returns true if a new one
  // was added.
  bool AddOrUpdateScanResult(const MediaGalleryPrefInfo& gallery,
                             bool selected,
                             views::View* container,
                             int trailing_vertical_space);

  void ShowContextMenu(const gfx::Point& point,
                       ui::MenuSourceType source_type,
                       MediaGalleryPrefId id);

  MediaGalleriesScanResultDialogController* controller_;

  // The containing window (a weak pointer).
  views::Widget* window_;

  // The contents of the dialog. Owned by |window_|'s RootView.
  views::View* contents_;

  // A map from media gallery ID to the view elements for each gallery.
  GalleryViewMap gallery_view_map_;

  // True if the user has pressed accept.
  bool accepted_;

  scoped_ptr<views::MenuRunner> context_menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesScanResultDialogViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_VIEWS_H_
