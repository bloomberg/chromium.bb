// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERIES_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERIES_DIALOG_VIEWS_H_

#include <map>

#include "base/compiler_specific.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class LabelButton;
class MenuRunner;
class Widget;
}

class MediaGalleryCheckboxView;

// The media galleries configuration view for Views. It will immediately show
// upon construction.
class MediaGalleriesDialogViews : public MediaGalleriesDialog,
                                  public views::ButtonListener,
                                  public views::ContextMenuController,
                                  public views::DialogDelegate {
 public:
  explicit MediaGalleriesDialogViews(
      MediaGalleriesDialogController* controller);
  virtual ~MediaGalleriesDialogViews();

  // MediaGalleriesDialog implementation:
  virtual void UpdateGalleries() OVERRIDE;

  // views::DialogDelegate implementation:
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual views::View* CreateExtraView() OVERRIDE;
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
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, InitializeCheckboxes);
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, ToggleCheckboxes);
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, UpdateAdds);
  FRIEND_TEST_ALL_PREFIXES(MediaGalleriesDialogTest, ForgetDeletes);

  typedef std::map<MediaGalleryPrefId, MediaGalleryCheckboxView*> CheckboxMap;

  // MediaGalleriesDialog implementation:
  virtual void AcceptDialogForTesting() OVERRIDE;

  void InitChildViews();

  // Adds a checkbox or updates an existing checkbox. Returns true if a new one
  // was added.
  bool AddOrUpdateGallery(
      const MediaGalleriesDialogController::Entry& gallery,
      views::View* container,
      int trailing_vertical_space);

  void ShowContextMenu(const gfx::Point& point,
                       ui::MenuSourceType source_type,
                       MediaGalleryPrefId id);

  // Whether |controller_| has a valid WebContents or not.
  // In unit tests, it may not.
  bool ControllerHasWebContents() const;

  MediaGalleriesDialogController* controller_;

  // The contents of the dialog. Owned by the view hierarchy, except in tests.
  views::View* contents_;

  // A map from gallery ID to views::Checkbox view.
  CheckboxMap checkbox_map_;

  // Pointer to the controller specific auxiliary button, NULL otherwise.
  // Owned by parent in the dialog views tree.
  views::LabelButton* auxiliary_button_;

  // This tracks whether the confirm button can be clicked. It starts as false
  // if no checkboxes are ticked. After there is any interaction, or some
  // checkboxes start checked, this will be true.
  bool confirm_available_;

  // True if the user has pressed accept.
  bool accepted_;

  scoped_ptr<views::MenuRunner> context_menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesDialogViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_MEDIA_GALLERIES_DIALOG_VIEWS_H_
