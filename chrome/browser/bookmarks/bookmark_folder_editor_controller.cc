// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_folder_editor_controller.h"

#include "app/l10n_util.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

BookmarkFolderEditorController::~BookmarkFolderEditorController() {
  if (model_)
    model_->RemoveObserver(this);
}

// static
void BookmarkFolderEditorController::Show(Profile* profile,
                                          gfx::NativeWindow wnd,
                                          const BookmarkNode* node,
                                          int index,
                                          uint32 details) {
  // BookmarkFolderEditorController deletes itself when done.
  BookmarkFolderEditorController* editor =
      new BookmarkFolderEditorController(profile, wnd, node, index, details);
  editor->Show();
}

BookmarkFolderEditorController::BookmarkFolderEditorController(
    Profile* profile,
    gfx::NativeWindow wnd,
    const BookmarkNode* node,
    int index,
    uint32 details)
    : profile_(profile),
      model_(profile->GetBookmarkModel()),
      node_(node),
      index_(index),
      details_(details) {
  DCHECK(IsNew() || node);

  std::wstring title = IsNew() ?
      l10n_util::GetString(IDS_BOOMARK_FOLDER_EDITOR_WINDOW_TITLE_NEW) :
      l10n_util::GetString(IDS_BOOMARK_FOLDER_EDITOR_WINDOW_TITLE);
  std::wstring label =
      l10n_util::GetString(IDS_BOOMARK_BAR_EDIT_FOLDER_LABEL);
  std::wstring contents = IsNew() ?
      l10n_util::GetString(IDS_BOOMARK_EDITOR_NEW_FOLDER_NAME) :
      UTF16ToWide(node_->GetTitleAsString16());

  dialog_ = InputWindowDialog::Create(wnd, title, label, contents, this);
  model_->AddObserver(this);
}

void BookmarkFolderEditorController::Show() {
  dialog_->Show();
}

bool BookmarkFolderEditorController::IsValid(const std::wstring& text) {
  return !text.empty();
}

void BookmarkFolderEditorController::InputAccepted(const std::wstring& text) {
  if (IsNew())
    model_->AddGroup(node_, index_, text);
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

bool BookmarkFolderEditorController::IsNew() {
  return (details_ & IS_NEW) != 0;
}
