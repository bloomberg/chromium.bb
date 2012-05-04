// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/android_history_provider_service.h"

#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/profiles/profile.h"

using history::HistoryBackend;

AndroidHistoryProviderService::AndroidHistoryProviderService(Profile* profile)
    : profile_(profile) {
}

AndroidHistoryProviderService::~AndroidHistoryProviderService() {
}

AndroidHistoryProviderService::Handle
AndroidHistoryProviderService::QueryHistoryAndBookmarks(
    const std::vector<history::HistoryAndBookmarkRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<string16>& selection_args,
    const std::string& sort_order,
    CancelableRequestConsumerBase* consumer,
    const QueryCallback& callback) {
  QueryRequest* request = new QueryRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->Schedule(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::QueryHistoryAndBookmarks, NULL, request,
            projections, selection, selection_args, sort_order);
  } else {
    request->ForwardResultAsync(request->handle(), false, 0);
  }
  return request->handle();
}

AndroidHistoryProviderService::Handle
AndroidHistoryProviderService::UpdateHistoryAndBookmarks(
    const history::HistoryAndBookmarkRow& row,
    const std::string& selection,
    const std::vector<string16>& selection_args,
    CancelableRequestConsumerBase* consumer,
    const UpdateCallback& callback) {
  UpdateRequest* request = new UpdateRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->Schedule(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::UpdateHistoryAndBookmarks, NULL, request, row,
            selection, selection_args);
  } else {
    request->ForwardResultAsync(request->handle(), false, 0);
  }
  return request->handle();
}

AndroidHistoryProviderService::Handle
AndroidHistoryProviderService::DeleteHistoryAndBookmarks(
    const std::string& selection,
    const std::vector<string16>& selection_args,
    CancelableRequestConsumerBase* consumer,
    const DeleteCallback& callback) {
  DeleteRequest* request = new DeleteRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->Schedule(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::DeleteHistoryAndBookmarks, NULL, request,
            selection, selection_args);
  } else {
    request->ForwardResultAsync(request->handle(), false, 0);
  }
  return request->handle();
}

AndroidHistoryProviderService::Handle
AndroidHistoryProviderService::InsertHistoryAndBookmark(
    const history::HistoryAndBookmarkRow& values,
    CancelableRequestConsumerBase* consumer,
    const InsertCallback& callback) {
  InsertRequest* request = new InsertRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->Schedule(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::InsertHistoryAndBookmark, NULL, request, values);
  } else {
    request->ForwardResultAsync(request->handle(), false, 0);
  }
  return request->handle();
}

AndroidHistoryProviderService::Handle
AndroidHistoryProviderService::DeleteHistory(
    const std::string& selection,
    const std::vector<string16>& selection_args,
    CancelableRequestConsumerBase* consumer,
    const DeleteCallback& callback) {
  DeleteRequest* request = new DeleteRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->Schedule(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::DeleteHistory, NULL, request, selection,
            selection_args);
  } else {
    request->ForwardResultAsync(request->handle(), false, 0);
  }
  return request->handle();
}

AndroidHistoryProviderService::Handle
AndroidHistoryProviderService::MoveStatement(
    history::AndroidStatement* statement,
    int current_pos,
    int destination,
    CancelableRequestConsumerBase* consumer,
    const MoveStatementCallback& callback) {
  MoveStatementRequest* request = new MoveStatementRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->Schedule(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::MoveStatement, NULL, request, statement,
            current_pos, destination);
  } else {
    request->ForwardResultAsync(request->handle(), current_pos);
  }
  return request->handle();
}

void AndroidHistoryProviderService::CloseStatement(
    history::AndroidStatement* statement) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->ScheduleAndForget(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::CloseStatement, statement);
  } else {
    delete statement;
  }
}

AndroidHistoryProviderService::Handle
AndroidHistoryProviderService::InsertSearchTerm(
    const history::SearchRow& row,
    CancelableRequestConsumerBase* consumer,
    const InsertCallback& callback) {
  InsertRequest* request = new InsertRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->Schedule(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::InsertSearchTerm, NULL, request, row);
  } else {
    request->ForwardResultAsync(request->handle(), false, 0);
  }
  return request->handle();
}

AndroidHistoryProviderService::Handle
AndroidHistoryProviderService::UpdateSearchTerms(
    const history::SearchRow& row,
    const std::string& selection,
    const std::vector<string16>& selection_args,
    CancelableRequestConsumerBase* consumer,
    const UpdateCallback& callback) {
  UpdateRequest* request = new UpdateRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->Schedule(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::UpdateSearchTerms, NULL, request, row, selection,
            selection_args);
  } else {
    request->ForwardResultAsync(request->handle(), false, 0);
  }
  return request->handle();
}

AndroidHistoryProviderService::Handle
AndroidHistoryProviderService::DeleteSearchTerms(
    const std::string& selection,
    const std::vector<string16>& selection_args,
    CancelableRequestConsumerBase* consumer,
    const DeleteCallback& callback) {
  DeleteRequest* request = new DeleteRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->Schedule(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::DeleteSearchTerms, NULL, request, selection,
            selection_args);
  } else {
    request->ForwardResultAsync(request->handle(), false, 0);
  }
  return request->handle();
}

AndroidHistoryProviderService::Handle
AndroidHistoryProviderService::QuerySearchTerms(
    const std::vector<history::SearchRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<string16>& selection_args,
    const std::string& sort_order,
    CancelableRequestConsumerBase* consumer,
    const QueryCallback& callback) {
  QueryRequest* request = new QueryRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->Schedule(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::QuerySearchTerms, NULL, request, projections,
            selection, selection_args, sort_order);
  } else {
    request->ForwardResultAsync(request->handle(), false, 0);
  }
  return request->handle();
}
