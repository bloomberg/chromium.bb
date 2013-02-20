// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTENSIONS_MEDIA_GALLERIES_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTENSIONS_MEDIA_GALLERIES_DIALOG_GTK_H_

#include <gtk/gtk.h>

#include <map>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/media_gallery/media_galleries_dialog_controller.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

namespace chrome {

class MediaGalleriesDialogController;
class MediaGalleriesDialogTest;

// The media galleries configuration view for Gtk. It will immediately show
// upon construction.
class MediaGalleriesDialogGtk : public MediaGalleriesDialog,
                                public ConstrainedWindowGtkDelegate {
 public:
  explicit MediaGalleriesDialogGtk(MediaGalleriesDialogController* controller);
  virtual ~MediaGalleriesDialogGtk();

  // MediaGalleriesDialog implementation:
  virtual void UpdateGallery(const MediaGalleryPrefInfo* gallery,
                             bool permitted) OVERRIDE;
  virtual void ForgetGallery(const MediaGalleryPrefInfo* gallery) OVERRIDE;

  // ConstrainedWindowGtkDelegate implementation:
  virtual GtkWidget* GetWidgetRoot() OVERRIDE;
  virtual GtkWidget* GetFocusWidget() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

  // Event callbacks.
  CHROMEGTK_CALLBACK_0(MediaGalleriesDialogGtk, void, OnToggled);
  CHROMEGTK_CALLBACK_0(MediaGalleriesDialogGtk, void, OnAddFolder);
  CHROMEGTK_CALLBACK_0(MediaGalleriesDialogGtk, void, OnConfirm);
  CHROMEGTK_CALLBACK_0(MediaGalleriesDialogGtk, void, OnCancel);

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, InitializeCheckboxes);
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, ToggleCheckboxes);
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, UpdateAdds);
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, ForgetDeletes);

  typedef std::map<const MediaGalleryPrefInfo*, GtkWidget*> CheckboxMap;

  // Creates the widget hierarchy.
  void InitWidgets();

  // Updates the state of the confirm button. It will be disabled when
  void UpdateConfirmButtonState();

  MediaGalleriesDialogController* controller_;
  ConstrainedWindowGtk* window_;

  // The root widget for the dialog.
  ui::OwnedWidgetGtk contents_;

  // The confirm button.
  GtkWidget* confirm_;

  // This widget holds all the checkboxes.
  GtkWidget* checkbox_container_;

  // A map from MediaGalleryPrefInfo struct (owned by controller) to the
  // GtkCheckButton that controls it.
  CheckboxMap checkbox_map_;

  // When true, checkbox toggles are ignored (used when updating the UI).
  bool ignore_toggles_;

  // True if the user has pressed accept.
  bool accepted_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesDialogGtk);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_GTK_EXTENSIONS_MEDIA_GALLERIES_DIALOG_GTK_H_
