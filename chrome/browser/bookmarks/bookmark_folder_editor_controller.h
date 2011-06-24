// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_FOLDER_EDITOR_CONTROLLER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_FOLDER_EDITOR_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/ui/input_window_dialog.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

// BookmarkFolderEditorController manages the editing and/or creation of a
// folder. If the user presses ok, the name change is committed to the model.
//
// BookmarkFolderEditorController deletes itself when the window is closed.
class BookmarkFolderEditorController : public InputWindowDialog::Delegate,
                                       public BaseBookmarkModelObserver {
 public:
  enum Type {
    NEW_BOOKMARK,  // Indicates that we are creating a new bookmark.
    EXISTING_BOOKMARK,  // Indicates that we are renaming an existing bookmark.
  };

  virtual ~BookmarkFolderEditorController();

  static void Show(Profile* profile,
                   gfx::NativeWindow wnd,
                   const BookmarkNode* node,
                   int index,
                   Type type);

 private:
  BookmarkFolderEditorController(Profile* profile,
                                 gfx::NativeWindow wnd,
                                 const BookmarkNode* node,
                                 int index,
                                 Type type);

  // InputWindowDialog::Delegate:
  virtual bool IsValid(const string16& text) OVERRIDE;
  virtual void InputAccepted(const string16& text) OVERRIDE;
  virtual void InputCanceled() OVERRIDE;

  // BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;

  Profile* profile_;

  BookmarkModel* model_;

  // If |is_new_| is true, this is the parent to create the new node under.
  // Otherwise this is the node to change the title of.
  const BookmarkNode* node_;

  // Index to insert the new folder at.
  int index_;

  const bool is_new_;

  InputWindowDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkFolderEditorController);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_FOLDER_EDITOR_CONTROLLER_H_
