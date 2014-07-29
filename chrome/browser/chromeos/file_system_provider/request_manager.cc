// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/request_manager.h"

#include "base/debug/trace_event.h"
#include "base/files/file.h"
#include "base/stl_util.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Timeout in seconds, before a request is considered as stale and hence
// aborted.
const int kDefaultTimeout = 10;

}  // namespace

std::string RequestTypeToString(RequestType type) {
  switch (type) {
    case REQUEST_UNMOUNT:
      return "REQUEST_UNMOUNT";
    case GET_METADATA:
      return "GET_METADATA";
    case READ_DIRECTORY:
      return "READ_DIRECTORY";
    case OPEN_FILE:
      return "OPEN_FILE";
    case CLOSE_FILE:
      return "CLOSE_FILE";
    case READ_FILE:
      return "READ_FILE";
    case CREATE_DIRECTORY:
      return "CREATE_DIRECTORY";
    case DELETE_ENTRY:
      return "DELETE_ENTRY";
    case CREATE_FILE:
      return "CREATE_FILE";
    case COPY_ENTRY:
      return "COPY_ENTRY";
    case MOVE_ENTRY:
      return "MOVE_ENTRY";
    case TRUNCATE:
      return "TRUNCATE";
    case WRITE_FILE:
      return "WRITE_FILE";
    case TESTING:
      return "TESTING";
  }
  NOTREACHED();
  return "";
}

RequestManager::RequestManager(
    NotificationManagerInterface* notification_manager)
    : notification_manager_(notification_manager),
      next_id_(1),
      timeout_(base::TimeDelta::FromSeconds(kDefaultTimeout)),
      weak_ptr_factory_(this) {
}

RequestManager::~RequestManager() {
  // Abort all of the active requests.
  RequestMap::iterator it = requests_.begin();
  while (it != requests_.end()) {
    const int request_id = it->first;
    ++it;
    RejectRequest(request_id,
                  scoped_ptr<RequestValue>(new RequestValue()),
                  base::File::FILE_ERROR_ABORT);
  }

  DCHECK_EQ(0u, requests_.size());
  STLDeleteValues(&requests_);
}

int RequestManager::CreateRequest(RequestType type,
                                  scoped_ptr<HandlerInterface> handler) {
  // The request id is unique per request manager, so per service, thereof
  // per profile.
  int request_id = next_id_++;

  // If cycled the int, then signal an error.
  if (requests_.find(request_id) != requests_.end())
    return 0;

  TRACE_EVENT_ASYNC_BEGIN1("file_system_provider",
                           "RequestManager::Request",
                           request_id,
                           "type",
                           type);

  Request* request = new Request;
  request->handler = handler.Pass();
  requests_[request_id] = request;
  ResetTimer(request_id);

  FOR_EACH_OBSERVER(Observer, observers_, OnRequestCreated(request_id, type));

  // Execute the request implementation. In case of an execution failure,
  // unregister and return 0. This may often happen, eg. if the providing
  // extension is not listening for the request event being sent.
  // In such case, we should abort as soon as possible.
  if (!request->handler->Execute(request_id)) {
    DestroyRequest(request_id);
    return 0;
  }

  FOR_EACH_OBSERVER(Observer, observers_, OnRequestExecuted(request_id));

  return request_id;
}

bool RequestManager::FulfillRequest(int request_id,
                                    scoped_ptr<RequestValue> response,
                                    bool has_more) {
  CHECK(response.get());
  RequestMap::iterator request_it = requests_.find(request_id);
  if (request_it == requests_.end())
    return false;

  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    OnRequestFulfilled(request_id, *response.get(), has_more));

  request_it->second->handler->OnSuccess(request_id, response.Pass(), has_more);

  if (!has_more) {
    DestroyRequest(request_id);
  } else {
    if (notification_manager_)
      notification_manager_->HideUnresponsiveNotification(request_id);
    ResetTimer(request_id);
  }

  return true;
}

bool RequestManager::RejectRequest(int request_id,
                                   scoped_ptr<RequestValue> response,
                                   base::File::Error error) {
  CHECK(response.get());
  RequestMap::iterator request_it = requests_.find(request_id);
  if (request_it == requests_.end())
    return false;

  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    OnRequestRejected(request_id, *response.get(), error));
  request_it->second->handler->OnError(request_id, response.Pass(), error);
  DestroyRequest(request_id);

  return true;
}

void RequestManager::SetTimeoutForTesting(const base::TimeDelta& timeout) {
  timeout_ = timeout;
}

size_t RequestManager::GetActiveRequestsForLogging() const {
  return requests_.size();
}

void RequestManager::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void RequestManager::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

RequestManager::Request::Request() {}

RequestManager::Request::~Request() {}

void RequestManager::OnRequestTimeout(int request_id) {
  FOR_EACH_OBSERVER(Observer, observers_, OnRequestTimeouted(request_id));

  if (!notification_manager_) {
    RejectRequest(request_id,
                  scoped_ptr<RequestValue>(new RequestValue()),
                  base::File::FILE_ERROR_ABORT);
    return;
  }

  notification_manager_->ShowUnresponsiveNotification(
      request_id,
      base::Bind(&RequestManager::OnUnresponsiveNotificationResult,
                 weak_ptr_factory_.GetWeakPtr(),
                 request_id));
}

void RequestManager::OnUnresponsiveNotificationResult(
    int request_id,
    NotificationManagerInterface::NotificationResult result) {
  RequestMap::iterator request_it = requests_.find(request_id);
  if (request_it == requests_.end())
    return;

  if (result == NotificationManagerInterface::CONTINUE) {
    ResetTimer(request_id);
    return;
  }

  RejectRequest(request_id,
                scoped_ptr<RequestValue>(new RequestValue()),
                base::File::FILE_ERROR_ABORT);
}

void RequestManager::ResetTimer(int request_id) {
  RequestMap::iterator request_it = requests_.find(request_id);
  if (request_it == requests_.end())
    return;

  request_it->second->timeout_timer.Start(
      FROM_HERE,
      timeout_,
      base::Bind(&RequestManager::OnRequestTimeout,
                 weak_ptr_factory_.GetWeakPtr(),
                 request_id));
}

void RequestManager::DestroyRequest(int request_id) {
  RequestMap::iterator request_it = requests_.find(request_id);
  if (request_it == requests_.end())
    return;

  delete request_it->second;
  requests_.erase(request_it);

  if (notification_manager_)
    notification_manager_->HideUnresponsiveNotification(request_id);

  FOR_EACH_OBSERVER(Observer, observers_, OnRequestDestroyed(request_id));

  TRACE_EVENT_ASYNC_END0(
      "file_system_provider", "RequestManager::Request", request_id);
}

}  // namespace file_system_provider
}  // namespace chromeos
