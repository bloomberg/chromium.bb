// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/request_manager.h"

#include "base/values.h"

namespace chromeos {
namespace file_system_provider {

RequestManager::RequestManager() : next_id_(1) {}

RequestManager::~RequestManager() {
  // Abort all of the active requests.
  RequestMap::iterator it = requests_.begin();
  while (it != requests_.end()) {
    const int request_id = it->first;
    ++it;
    RejectRequest(request_id, base::File::FILE_ERROR_ABORT);
  }
}

int RequestManager::CreateRequest(const SuccessCallback& success_callback,
                                  const ErrorCallback& error_callback) {
  // The request id is unique per request manager, so per service, thereof
  // per profile.
  int request_id = next_id_++;

  // If cycled the int, then signal an error.
  if (requests_.find(request_id) != requests_.end())
    return 0;

  Request request;
  request.success_callback = success_callback;
  request.error_callback = error_callback;
  requests_[request_id] = request;

  return request_id;
}

bool RequestManager::FulfillRequest(int request_id,
                                    scoped_ptr<base::DictionaryValue> response,
                                    bool has_next) {
  RequestMap::iterator request_it = requests_.find(request_id);

  if (request_it == requests_.end())
    return false;

  if (!request_it->second.success_callback.is_null())
    request_it->second.success_callback.Run(response.Pass(), has_next);
  if (!has_next)
    requests_.erase(request_it);

  return true;
}

bool RequestManager::RejectRequest(int request_id, base::File::Error error) {
  RequestMap::iterator request_it = requests_.find(request_id);

  if (request_it == requests_.end())
    return false;

  if (!request_it->second.error_callback.is_null())
    request_it->second.error_callback.Run(error);
  requests_.erase(request_it);

  return true;
}

RequestManager::Request::Request() {}

RequestManager::Request::~Request() {}

}  // namespace file_system_provider
}  // namespace chromeos
