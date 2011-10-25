// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_folder_editor_controller.h"

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

BookmarkFolderEditorController::~BookmarkFolderEditorController() {
  if (model_)
    model_->RemoveObserver(this);
}

// static
void BookmarkFolderEditorController::Show(Profile* profile,
                                          gfx::NativeWindow wnd,
                                          const BookmarkNode* node,
                                          int index,
                                          Type type) {
  // BookmarkFolderEditorController deletes itself when done.
  new BookmarkFolderEditorController(profile, wnd, node, index, type);
}

BookmarkFolderEditorController::BookmarkFolderEditorController(
    Profile* profile,
    gfx::NativeWindow wnd,
    const BookmarkNode* node,
    int index,
    Type type)
    : profile_(profile),
      model_(profile->GetBookmarkModel()),
      node_(node),
      index_(index),
      is_new_(type == NEW_BOOKMARK) {
  DCHECK(is_new_ || node);

  model_->AddObserver(this);

  string16 title = is_new_ ?
      l10n_util::GetStringUTF16(IDS_BOOKMARK_FOLDER_EDITOR_WINDOW_TITLE_NEW) :
      l10n_util::GetStringUTF16(IDS_BOOKMARK_FOLDER_EDITOR_WINDOW_TITLE);
  string16 label =
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_EDIT_FOLDER_LABEL);
  string16 contents = is_new_ ?
      l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME) :
      node_->GetTitle();

  dialog_ = InputWindowDialog::Create(wnd, title, label, contents, this);
  dialog_->Show();
}

bool BookmarkFolderEditorController::IsValid(const string16& text) {
  return !text.empty();
}

void BookmarkFolderEditorController::InputAccepted(const string16& text) {
  if (is_new_)
    model_->AddFolder(node_, index_, text);
  else
    model_->SetTitle(node_, text);
}

void BookmarkFolderEditorController::InputCanceled() {
}

void BookmarkFolderEditorController::BookmarkModelChanged() {
  dialog_->Close();
}

void BookmarkFolderEditorController::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  model_->RemoveObserver(this);
  model_ = NULL;
  BookmarkModelChanged();
}
