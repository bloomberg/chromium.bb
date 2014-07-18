// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_backend.h"

#include "chrome/browser/history/android/android_provider_backend.h"

namespace history {

AndroidURLID HistoryBackend::InsertHistoryAndBookmark(
    const HistoryAndBookmarkRow& row) {
  AndroidURLID id = 0;
  if (android_provider_backend_)
    id = android_provider_backend_->InsertHistoryAndBookmark(row);
  return id;
}

AndroidStatement* HistoryBackend::QueryHistoryAndBookmarks(
    const std::vector<HistoryAndBookmarkRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const std::string& sort_order) {
  AndroidStatement* statement = NULL;
  if (android_provider_backend_) {
    statement = android_provider_backend_->QueryHistoryAndBookmarks(
        projections, selection, selection_args, sort_order);
  }
  return statement;
}

int HistoryBackend::UpdateHistoryAndBookmarks(
    const HistoryAndBookmarkRow& row,
    const std::string& selection,
    const std::vector<base::string16>& selection_args) {
  int count = 0;
  if (android_provider_backend_) {
    android_provider_backend_->UpdateHistoryAndBookmarks(
        row, selection, selection_args, &count);
  }
  return count;
}

int HistoryBackend::DeleteHistoryAndBookmarks(
    const std::string& selection,
    const std::vector<base::string16>& selection_args) {
  int count = 0;
  if (android_provider_backend_) {
    android_provider_backend_->DeleteHistoryAndBookmarks(
        selection, selection_args, &count);
  }
  return count;
}

int HistoryBackend::DeleteHistory(
    const std::string& selection,
    const std::vector<base::string16>& selection_args) {
  int count = 0;
  if (android_provider_backend_) {
    android_provider_backend_->DeleteHistory(selection, selection_args, &count);
  }
  return count;
}

// Statement -------------------------------------------------------------------

int HistoryBackend::MoveStatement(history::AndroidStatement* statement,
                                  int current_pos,
                                  int destination) {
  DCHECK_LE(-1, current_pos);
  DCHECK_LE(-1, destination);

  int cur = current_pos;
  if (current_pos > destination) {
    statement->statement()->Reset(false);
    cur = -1;
  }
  for (; cur < destination; ++cur) {
    if (!statement->statement()->Step())
      break;
  }

  return cur;
}

void HistoryBackend::CloseStatement(AndroidStatement* statement) {
  delete statement;
}

// Search Term -----------------------------------------------------------------

SearchTermID HistoryBackend::InsertSearchTerm(const SearchRow& row) {
  SearchTermID id = 0;
  if (android_provider_backend_)
    id = android_provider_backend_->InsertSearchTerm(row);
  return id;
}

int HistoryBackend::UpdateSearchTerms(
    const SearchRow& row,
    const std::string& selection,
    const std::vector<base::string16> selection_args) {
  int count = 0;
  if (android_provider_backend_) {
    android_provider_backend_->UpdateSearchTerms(
        row, selection, selection_args, &count);
  }
  return count;
}

int HistoryBackend::DeleteSearchTerms(
    const std::string& selection,
    const std::vector<base::string16> selection_args) {
  int count = 0;
  if (android_provider_backend_) {
    android_provider_backend_->DeleteSearchTerms(
        selection, selection_args, &count);
  }
  return count;
}

AndroidStatement* HistoryBackend::QuerySearchTerms(
    const std::vector<SearchRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const std::string& sort_order) {
  AndroidStatement* statement = NULL;
  if (android_provider_backend_) {
    statement = android_provider_backend_->QuerySearchTerms(projections,
        selection, selection_args, sort_order);
  }
  return statement;
}

}  // namespace history
