// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/android_history_provider_service.h"

#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"

using history::HistoryBackend;

AndroidHistoryProviderService::AndroidHistoryProviderService(Profile* profile)
    : profile_(profile) {
}

AndroidHistoryProviderService::~AndroidHistoryProviderService() {
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::QueryHistoryAndBookmarks(
    const std::vector<history::HistoryAndBookmarkRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const std::string& sort_order,
    const QueryCallback& callback,
    base::CancelableTaskTracker* tracker) {
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (hs) {
    DCHECK(hs->thread_) << "History service being called after cleanup";
    DCHECK(hs->thread_checker_.CalledOnValidThread());
    return tracker->PostTaskAndReplyWithResult(
        hs->thread_->message_loop_proxy().get(),
        FROM_HERE,
        base::Bind(&HistoryBackend::QueryHistoryAndBookmarks,
                   hs->history_backend_.get(),
                   projections,
                   selection,
                   selection_args,
                   sort_order),
        callback);
  } else {
    callback.Run(NULL);
    return base::CancelableTaskTracker::kBadTaskId;
  }
}

AndroidHistoryProviderService::Handle
AndroidHistoryProviderService::UpdateHistoryAndBookmarks(
    const history::HistoryAndBookmarkRow& row,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    CancelableRequestConsumerBase* consumer,
    const UpdateCallback& callback) {
  UpdateRequest* request = new UpdateRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
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
    const std::vector<base::string16>& selection_args,
    CancelableRequestConsumerBase* consumer,
    const DeleteCallback& callback) {
  DeleteRequest* request = new DeleteRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
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
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
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
    const std::vector<base::string16>& selection_args,
    CancelableRequestConsumerBase* consumer,
    const DeleteCallback& callback) {
  DeleteRequest* request = new DeleteRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->Schedule(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::DeleteHistory, NULL, request, selection,
            selection_args);
  } else {
    request->ForwardResultAsync(request->handle(), false, 0);
  }
  return request->handle();
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::MoveStatement(
    history::AndroidStatement* statement,
    int current_pos,
    int destination,
    const MoveStatementCallback& callback,
    base::CancelableTaskTracker* tracker) {
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (hs) {
    DCHECK(hs->thread_) << "History service being called after cleanup";
    DCHECK(hs->thread_checker_.CalledOnValidThread());
    return tracker->PostTaskAndReplyWithResult(
        hs->thread_->message_loop_proxy().get(),
        FROM_HERE,
        base::Bind(&HistoryBackend::MoveStatement,
                   hs->history_backend_.get(),
                   statement,
                   current_pos,
                   destination),
        callback);
  } else {
    callback.Run(current_pos);
    return base::CancelableTaskTracker::kBadTaskId;
  }
}

void AndroidHistoryProviderService::CloseStatement(
    history::AndroidStatement* statement) {
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
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
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
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
    const std::vector<base::string16>& selection_args,
    CancelableRequestConsumerBase* consumer,
    const UpdateCallback& callback) {
  UpdateRequest* request = new UpdateRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
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
    const std::vector<base::string16>& selection_args,
    CancelableRequestConsumerBase* consumer,
    const DeleteCallback& callback) {
  DeleteRequest* request = new DeleteRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (hs) {
    hs->Schedule(HistoryService::PRIORITY_NORMAL,
            &HistoryBackend::DeleteSearchTerms, NULL, request, selection,
            selection_args);
  } else {
    request->ForwardResultAsync(request->handle(), false, 0);
  }
  return request->handle();
}

base::CancelableTaskTracker::TaskId
AndroidHistoryProviderService::QuerySearchTerms(
    const std::vector<history::SearchRow::ColumnID>& projections,
    const std::string& selection,
    const std::vector<base::string16>& selection_args,
    const std::string& sort_order,
    const QueryCallback& callback,
    base::CancelableTaskTracker* tracker) {
  HistoryService* hs =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (hs) {
    DCHECK(hs->thread_) << "History service being called after cleanup";
    DCHECK(hs->thread_checker_.CalledOnValidThread());
    return tracker->PostTaskAndReplyWithResult(
        hs->thread_->message_loop_proxy().get(),
        FROM_HERE,
        base::Bind(&HistoryBackend::QuerySearchTerms,
                   hs->history_backend_.get(),
                   projections,
                   selection,
                   selection_args,
                   sort_order),
        callback);
  } else {
    callback.Run(NULL);
    return base::CancelableTaskTracker::kBadTaskId;
  }
}
