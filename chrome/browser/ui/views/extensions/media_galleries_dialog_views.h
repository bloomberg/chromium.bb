// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERIES_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERIES_DIALOG_VIEWS_H_

#include <map>

#include "base/compiler_specific.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

class ConstrainedWindowViews;

namespace views {
class Checkbox;
class TextButton;
}

namespace chrome {

// The media galleries configuration view for Views. It will immediately show
// upon construction.
class MediaGalleriesDialogViews : public MediaGalleriesDialog,
                                  public views::DialogDelegate,
                                  public views::ButtonListener {
 public:
  explicit MediaGalleriesDialogViews(
      MediaGalleriesDialogController* controller);
  virtual ~MediaGalleriesDialogViews();

  // MediaGalleriesDialog implementation:
  virtual void UpdateGallery(const MediaGalleryPrefInfo* gallery,
                             bool permitted) OVERRIDE;
  virtual void ForgetGallery(const MediaGalleryPrefInfo* gallery) OVERRIDE;

  // views::DialogDelegate implementation:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual bool ShouldShowWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual views::View* CreateExtraView() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  typedef std::map<const MediaGalleryPrefInfo*, views::Checkbox*> CheckboxMap;

  void InitChildViews();

  // Adds a checkbox or updates an existing checkbox. Returns true if a new one
  // was added.
  bool AddOrUpdateGallery(const MediaGalleryPrefInfo* gallery, bool permitted);

  MediaGalleriesDialogController* controller_;

  // The constrained window (a weak pointer).
  ConstrainedWindowViews* window_;

  // The contents of the dialog. Owned by |window_|'s RootView.
  views::View* contents_;

  // A map from media gallery to views::Checkbox view.
  CheckboxMap checkbox_map_;

  views::View* checkbox_container_;
  views::TextButton* add_gallery_button_;

  // This tracks whether the confirm button can be clicked. It starts as false
  // if no checkboxes are ticked. After there is any interaction, or some
  // checkboxes start checked, this will be true.
  bool confirm_available_;

  // True if the user has pressed accept.
  bool accepted_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesDialogViews);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERIES_DIALOG_VIEWS_H_
