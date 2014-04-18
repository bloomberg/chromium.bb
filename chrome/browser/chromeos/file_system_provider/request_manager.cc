// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/request_manager.h"

#include "base/files/file.h"
#include "base/stl_util.h"
#include "base/values.h"

namespace chromeos {
namespace file_system_provider {

namespace {

// Timeout in seconds, before a request is considered as stale and hence
// aborted.
const int kDefaultTimeout = 10;

}  // namespace

RequestManager::RequestManager()
    : next_id_(1),
      timeout_(base::TimeDelta::FromSeconds(kDefaultTimeout)),
      weak_ptr_factory_(this) {}

RequestManager::~RequestManager() {
  // Abort all of the active requests.
  RequestMap::iterator it = requests_.begin();
  while (it != requests_.end()) {
    const int request_id = it->first;
    ++it;
    RejectRequest(request_id, base::File::FILE_ERROR_ABORT);
  }

  DCHECK_EQ(0u, requests_.size());
  STLDeleteValues(&requests_);
}

int RequestManager::CreateRequest(const SuccessCallback& success_callback,
                                  const ErrorCallback& error_callback) {
  // The request id is unique per request manager, so per service, thereof
  // per profile.
  int request_id = next_id_++;

  // If cycled the int, then signal an error.
  if (requests_.find(request_id) != requests_.end())
    return 0;

  Request* request = new Request;
  request->success_callback = success_callback;
  request->error_callback = error_callback;
  request->timeout_timer.Start(FROM_HERE,
                               timeout_,
                               base::Bind(&RequestManager::OnRequestTimeout,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          request_id));
  requests_[request_id] = request;

  return request_id;
}

bool RequestManager::FulfillRequest(int request_id,
                                    scoped_ptr<base::DictionaryValue> response,
                                    bool has_next) {
  RequestMap::iterator request_it = requests_.find(request_id);

  if (request_it == requests_.end())
    return false;

  if (!request_it->second->success_callback.is_null())
    request_it->second->success_callback.Run(response.Pass(), has_next);
  if (!has_next) {
    delete request_it->second;
    requests_.erase(request_it);
  } else {
    request_it->second->timeout_timer.Reset();
  }

  return true;
}

bool RequestManager::RejectRequest(int request_id, base::File::Error error) {
  RequestMap::iterator request_it = requests_.find(request_id);

  if (request_it == requests_.end())
    return false;

  if (!request_it->second->error_callback.is_null())
    request_it->second->error_callback.Run(error);
  delete request_it->second;
  requests_.erase(request_it);

  return true;
}

void RequestManager::SetTimeoutForTests(const base::TimeDelta& timeout) {
  timeout_ = timeout;
}

void RequestManager::OnRequestTimeout(int request_id) {
  RejectRequest(request_id, base::File::FILE_ERROR_ABORT);
}

RequestManager::Request::Request() {}

RequestManager::Request::~Request() {}

}  // namespace file_system_provider
}  // namespace chromeos
