// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_INPUT_WINDOW_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_INPUT_WINDOW_DIALOG_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/input_window_dialog.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

// BookmarkInputWindowDialogController manages the editing and/or creation of a
// bookmark page or a bookmark folder. If the user presses ok, the name change
// is committed to the model.
//
// BookmarkInputWindowDialogController deletes itself when the window is closed.
class BookmarkInputWindowDialogController : public InputWindowDialog::Delegate,
                                            public BaseBookmarkModelObserver {
 public:
  virtual ~BookmarkInputWindowDialogController();

  static void Show(Profile* profile,
                   gfx::NativeWindow wnd,
                   const BookmarkEditor::EditDetails& details);

 private:
  BookmarkInputWindowDialogController(
      Profile* profile,
      gfx::NativeWindow wnd,
      const BookmarkEditor::EditDetails& details);

  // InputWindowDialog::Delegate:
  virtual bool IsValid(const InputWindowDialog::InputTexts& texts) OVERRIDE;
  virtual void InputAccepted(
      const InputWindowDialog::InputTexts& texts) OVERRIDE;
  virtual void InputCanceled() OVERRIDE;

  // BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;

  BookmarkModel* model_;

  BookmarkEditor::EditDetails details_;

  InputWindowDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkInputWindowDialogController);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_INPUT_WINDOW_DIALOG_CONTROLLER_H_
