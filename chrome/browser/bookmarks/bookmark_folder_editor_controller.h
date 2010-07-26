// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_FOLDER_EDITOR_CONTROLLER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_FOLDER_EDITOR_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/input_window_dialog.h"
#include "gfx/native_widget_types.h"

class Profile;

// BookmarkFolderEditorController manages the editing and/or creation of a
// folder. If the user presses ok, the name change is committed to the model.
//
// BookmarkFolderEditorController deletes itself when the window is closed.
class BookmarkFolderEditorController : public InputWindowDialog::Delegate,
                                       public BaseBookmarkModelObserver {
 public:
  enum Details {
    NONE            = 1 << 0,
    IS_NEW          = 1 << 1,
    SHOW_IN_MANAGER = 1 << 2,
  };

  virtual ~BookmarkFolderEditorController();

  // |details| is a bitmask of Details (see above).
  static void Show(Profile* profile,
                   gfx::NativeWindow wnd,
                   const BookmarkNode* node,
                   int index,
                   uint32 details);

 private:
  BookmarkFolderEditorController(Profile* profile,
                                 gfx::NativeWindow wnd,
                                 const BookmarkNode* node,
                                 int index,
                                 uint32 details);
  void Show();

  // Overriden from InputWindowDialog::Delegate:
  virtual bool IsValid(const std::wstring& text);
  virtual void InputAccepted(const std::wstring& text);
  virtual void InputCanceled();

  // Overridden from BaseBookmarkModelObserver:
  virtual void BookmarkModelChanged();
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model);

  // Returns true if we are creating a new bookmark folder, otherwise returns
  // false if we are editing the bookmark folder.
  bool IsNew();

  Profile* profile_;

  BookmarkModel* model_;

  // If IsNew() is true, this is the parent to create the new node under.
  // Otherwise this is the node to change the title of.
  const BookmarkNode* node_;

  // Index to insert the new folder at.
  int index_;

  // The bitmask of Details (see the enum above).
  const uint32 details_;

  InputWindowDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkFolderEditorController);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_FOLDER_EDITOR_CONTROLLER_H_
