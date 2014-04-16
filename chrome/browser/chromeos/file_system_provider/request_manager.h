// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REQUEST_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REQUEST_MANAGER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace chromeos {
namespace file_system_provider {

typedef base::Callback<void(scoped_ptr<base::DictionaryValue> result,
                            bool has_next)> SuccessCallback;
typedef base::Callback<void(base::File::Error)> ErrorCallback;

// Manages requests between the service, async utils and the providing
// extensions.
class RequestManager {
 public:
  RequestManager();
  virtual ~RequestManager();

  // Creates a request and returns its request id (greater than 0). Returns 0 in
  // case of an error (eg. too many requests). The passed callbacks can be NULL.
  int CreateRequest(const SuccessCallback& success_callback,
                    const ErrorCallback& error_callback);

  // Handles successful response for the |request_id|. If |has_next| is false,
  // then the request is disposed, after handling the |response|. On error,
  // returns false, and the request is disposed.
  bool FulfillRequest(int request_id,
                      scoped_ptr<base::DictionaryValue> response,
                      bool has_next);

  // Handles error response for the |request_id|. If handling the error fails,
  // returns false. Always disposes the request.
  bool RejectRequest(int request_id, base::File::Error error);

 private:
  struct Request {
    Request();
    ~Request();

    // Callback to be called on success.
    SuccessCallback success_callback;

    // Callback to be called on error.
    ErrorCallback error_callback;
  };

  typedef std::map<int, Request> RequestMap;

  RequestMap requests_;
  int next_id_;

  DISALLOW_COPY_AND_ASSIGN(RequestManager);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REQUEST_MANAGER_H_
