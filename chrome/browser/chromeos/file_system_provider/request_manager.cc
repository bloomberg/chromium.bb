// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/request_manager.h"

#include "base/values.h"

namespace chromeos {
namespace file_system_provider {

RequestManager::RequestManager() : next_id_(1) {}

RequestManager::~RequestManager() {}

int RequestManager::CreateRequest(const std::string& extension_id,
                                  int file_system_id,
                                  const SuccessCallback& success_callback,
                                  const ErrorCallback& error_callback) {
  // The request id is unique per request manager, so per service, thereof
  // per profile.
  int request_id = next_id_++;

  // If cycled the int, then signal an error.
  if (requests_.find(request_id) != requests_.end())
    return 0;

  Request request;
  request.extension_id = extension_id;
  request.file_system_id = file_system_id;
  request.success_callback = success_callback;
  request.error_callback = error_callback;
  requests_[request_id] = request;

  return request_id;
}

bool RequestManager::FulfillRequest(const std::string& extension_id,
                                    int file_system_id,
                                    int request_id,
                                    scoped_ptr<base::DictionaryValue> response,
                                    bool has_next) {
  RequestMap::iterator request_it = requests_.find(request_id);

  if (request_it == requests_.end())
    return false;

  // Check if the request belongs to the same provided file system.
  if (request_it->second.file_system_id != file_system_id ||
      request_it->second.extension_id != extension_id) {
    return false;
  }

  if (!request_it->second.success_callback.is_null())
    request_it->second.success_callback.Run(response.Pass(), has_next);
  if (!has_next)
    requests_.erase(request_it);

  return true;
}

bool RequestManager::RejectRequest(const std::string& extension_id,
                                   int file_system_id,
                                   int request_id,
                                   base::File::Error error) {
  RequestMap::iterator request_it = requests_.find(request_id);

  if (request_it == requests_.end())
    return false;

  // Check if the request belongs to the same provided file system.
  if (request_it->second.file_system_id != file_system_id ||
      request_it->second.extension_id != extension_id) {
    return false;
  }

  if (!request_it->second.error_callback.is_null())
    request_it->second.error_callback.Run(error);
  requests_.erase(request_it);

  return true;
}

void RequestManager::OnProvidedFileSystemMount(
    const ProvidedFileSystemInfo& file_system_info,
    base::File::Error error) {}

void RequestManager::OnProvidedFileSystemUnmount(
    const ProvidedFileSystemInfo& file_system_info,
    base::File::Error error) {
  // Do not continue on error, since the volume may be still mounted.
  if (error != base::File::FILE_OK)
    return;

  // Remove all requests for this provided file system.
  RequestMap::iterator it = requests_.begin();
  while (it != requests_.begin()) {
    if (it->second.file_system_id == file_system_info.file_system_id() &&
        it->second.extension_id == file_system_info.extension_id()) {
      RejectRequest(it->second.extension_id,
                    it->second.file_system_id,
                    it->first,
                    base::File::FILE_ERROR_ABORT);
      requests_.erase(it++);
    } else {
      ++it;
    }
  }
}

RequestManager::Request::Request() : file_system_id(0) {}

RequestManager::Request::~Request() {}

}  // namespace file_system_provider
}  // namespace chromeos
