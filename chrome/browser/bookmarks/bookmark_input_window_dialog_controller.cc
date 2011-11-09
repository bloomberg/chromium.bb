// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_input_window_dialog_controller.h"

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
string16 GetWindowTitle(const BookmarkEditor::EditDetails& details) {
  int title_id = 0;
  switch (details.type) {
    case BookmarkEditor::EditDetails::EXISTING_NODE:
      if (details.existing_node->is_url()) {
        title_id = IDS_BOOKMARK_EDITOR_TITLE;
      } else {
        title_id = IDS_BOOKMARK_FOLDER_EDITOR_WINDOW_TITLE;
      }
      break;
    case BookmarkEditor::EditDetails::NEW_URL:
      title_id = IDS_BOOKMARK_EDITOR_TITLE;
      break;
    case BookmarkEditor::EditDetails::NEW_FOLDER:
      title_id = IDS_BOOKMARK_FOLDER_EDITOR_WINDOW_TITLE_NEW;
      break;
    default:
      NOTREACHED();
      break;
  }
  return l10n_util::GetStringUTF16(title_id);
}
}  // namespace

BookmarkInputWindowDialogController::~BookmarkInputWindowDialogController() {
  if (model_)
    model_->RemoveObserver(this);
}

// static
void BookmarkInputWindowDialogController::Show(
    Profile* profile,
    gfx::NativeWindow wnd,
    const BookmarkEditor::EditDetails& details) {
  // BookmarkInputWindowDialogController deletes itself when done.
  new BookmarkInputWindowDialogController(profile, wnd, details);
}

BookmarkInputWindowDialogController::BookmarkInputWindowDialogController(
    Profile* profile,
    gfx::NativeWindow wnd,
    const BookmarkEditor::EditDetails& details)
    : profile_(profile),
      model_(profile->GetBookmarkModel()),
      details_(details) {

  model_->AddObserver(this);

  InputWindowDialog::LabelContentsPairs label_contents_pairs;
  string16 name_label =
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_EDIT_FOLDER_LABEL);
  if ((details.type == BookmarkEditor::EditDetails::EXISTING_NODE &&
       details.existing_node->is_url()) ||
      details.type == BookmarkEditor::EditDetails::NEW_URL) {
    string16 url_label =
        l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_URL_LABEL);
    string16 name;
    GURL url;
    if (details.type == BookmarkEditor::EditDetails::NEW_URL) {
      bookmark_utils::GetURLAndTitleToBookmarkFromCurrentTab(profile,
                                                             &url,
                                                             &name);
    } else {
      url = details.existing_node->url();
      name = details.existing_node->GetTitle();
    }
    label_contents_pairs.push_back(std::make_pair(name_label, name));
    label_contents_pairs.push_back(std::make_pair(url_label,
                                                  UTF8ToUTF16(url.spec())));
  } else {
    string16 name = (details.type == BookmarkEditor::EditDetails::NEW_FOLDER) ?
      l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME) :
      details.existing_node->GetTitle();
    label_contents_pairs.push_back(std::make_pair(name_label, name));
  }

  const InputWindowDialog::ButtonType button_type =
      (details.type == BookmarkEditor::EditDetails::NEW_URL ||
       details.type == BookmarkEditor::EditDetails::NEW_FOLDER) ?
      InputWindowDialog::BUTTON_TYPE_ADD : InputWindowDialog::BUTTON_TYPE_SAVE;
  dialog_ = InputWindowDialog::Create(wnd,
                                      GetWindowTitle(details),
                                      label_contents_pairs, this, button_type);
  dialog_->Show();
}

bool BookmarkInputWindowDialogController::IsValid(
    const InputWindowDialog::InputTexts& texts) {
  if (texts.size() != 1 && texts.size() != 2) {
    return false;
  }
  if (texts[0].empty() || (texts.size() == 2 && texts[1].empty())) {
    return false;
  }
  return true;
}

void BookmarkInputWindowDialogController::InputAccepted(
    const InputWindowDialog::InputTexts& texts) {
  if (details_.type == BookmarkEditor::EditDetails::EXISTING_NODE) {
    if (details_.existing_node->is_url()) {
      DCHECK_EQ(2U, texts.size());
      model_->SetTitle(details_.existing_node, texts[0]);
      model_->SetURL(details_.existing_node, GURL(texts[1]));
    } else {
      DCHECK_EQ(1U, texts.size());
      model_->SetTitle(details_.existing_node, texts[0]);
    }
  } else if (details_.type == BookmarkEditor::EditDetails::NEW_FOLDER) {
    DCHECK_EQ(1U, texts.size());
    // TODO(mazda): Add |details_.urls| to AddFolder.
    model_->AddFolder(details_.parent_node, details_.index, texts[0]);
  } else if (details_.type == BookmarkEditor::EditDetails::NEW_URL) {
    DCHECK_EQ(2U, texts.size());
    model_->AddURL(details_.parent_node, details_.index, texts[0],
                   GURL(texts[1]));
  }
}

void BookmarkInputWindowDialogController::InputCanceled() {
}

void BookmarkInputWindowDialogController::BookmarkModelChanged() {
  dialog_->Close();
}

void BookmarkInputWindowDialogController::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  model_->RemoveObserver(this);
  model_ = NULL;
  BookmarkModelChanged();
}
