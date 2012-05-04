// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_backend.h"

#include "chrome/browser/history/android/android_provider_backend.h"

namespace history {

void HistoryBackend::InsertHistoryAndBookmark(
    scoped_refptr<InsertRequest> request,
    const HistoryAndBookmarkRow& row) {
  if (request->canceled())
    return;

  AndroidURLID id = 0;
  if (android_provider_backend_.get())
    id = android_provider_backend_->InsertHistoryAndBookmark(row);

  request->ForwardResult(request->handle(), id != 0, id);
}

void HistoryBackend::QueryHistoryAndBookmarks(
    scoped_refptr<QueryRequest> request,
    const std::vector<HistoryAndBookmarkRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<string16>& selection_args,
    const std::string& sort_order) {
  if (request->canceled())
    return;

  AndroidStatement* statement = NULL;
  if (android_provider_backend_.get()) {
    statement = android_provider_backend_->QueryHistoryAndBookmarks(
        projections, selection, selection_args, sort_order);
  }
  request->ForwardResult(request->handle(), statement, statement);
}

void HistoryBackend::UpdateHistoryAndBookmarks(
    scoped_refptr<UpdateRequest> request,
    const HistoryAndBookmarkRow& row,
    const std::string& selection,
    const std::vector<string16>& selection_args) {
  if (request->canceled())
    return;

  int count = 0;
  bool result = false;
  if (android_provider_backend_.get()) {
    result = android_provider_backend_->UpdateHistoryAndBookmarks(row,
        selection, selection_args, &count);
  }

  request->ForwardResult(request->handle(), result, count);
}

void HistoryBackend::DeleteHistoryAndBookmarks(
    scoped_refptr<DeleteRequest> request,
    const std::string& selection,
    const std::vector<string16>& selection_args) {
  if (request->canceled())
    return;

  int count = 0;
  bool result = false;
  if (android_provider_backend_.get())
    result = android_provider_backend_->DeleteHistoryAndBookmarks(selection,
        selection_args, &count);

  request->ForwardResult(request->handle(), result, count);
}

void HistoryBackend::DeleteHistory(
    scoped_refptr<DeleteRequest> request,
    const std::string& selection,
    const std::vector<string16>& selection_args) {
  if (request->canceled())
    return;

  int count = 0;
  bool result = false;
  if (android_provider_backend_.get()) {
    result = android_provider_backend_->DeleteHistory(selection, selection_args,
                                                      &count);
  }
  request->ForwardResult(request->handle(), result, count);
}

// Statement -------------------------------------------------------------------

void HistoryBackend::MoveStatement(
    scoped_refptr<MoveStatementRequest> request,
    history::AndroidStatement* statement,
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

  request->ForwardResult(request->handle(), cur);
}

void HistoryBackend::CloseStatement(AndroidStatement* statement) {
  delete statement;
}

// Search Term -----------------------------------------------------------------

void HistoryBackend::InsertSearchTerm(scoped_refptr<InsertRequest> request,
                                      const SearchRow& row) {
  if (request->canceled())
    return;

  SearchTermID id = 0;
  if (android_provider_backend_.get())
    id = android_provider_backend_->InsertSearchTerm(row);

  request->ForwardResult(request->handle(), id != 0, id);
}

void HistoryBackend::UpdateSearchTerms(
    scoped_refptr<UpdateRequest> request,
    const SearchRow& row,
    const std::string& selection,
    const std::vector<string16> selection_args) {
  if (request->canceled())
    return;

  int count = 0;
  bool result = false;
  if (android_provider_backend_.get()) {
    result =  android_provider_backend_->UpdateSearchTerms(row, selection,
        selection_args, &count);
  }
  request->ForwardResult(request->handle(), result, count);
}

void HistoryBackend::DeleteSearchTerms(
    scoped_refptr<DeleteRequest> request,
    const std::string& selection,
    const std::vector<string16> selection_args) {
  if (request->canceled())
    return;

  int count = 0;
  bool result = false;
  if (android_provider_backend_.get()) {
    result = android_provider_backend_->DeleteSearchTerms(selection,
        selection_args, &count);
  }

  request->ForwardResult(request->handle(), result, count);
}

void HistoryBackend::QuerySearchTerms(
    scoped_refptr<QueryRequest> request,
    const std::vector<SearchRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<string16>& selection_args,
    const std::string& sort_order) {
  if (request->canceled())
    return;

  AndroidStatement* statement = NULL;
  if (android_provider_backend_.get())
    statement = android_provider_backend_->QuerySearchTerms(projections,
        selection, selection_args, sort_order);

  request->ForwardResult(request->handle(), statement, statement);
}

}  // namespace history
