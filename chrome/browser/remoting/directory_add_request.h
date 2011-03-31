// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REMOTING_DIRECTORY_ADD_REQUEST_H_
#define CHROME_BROWSER_REMOTING_DIRECTORY_ADD_REQUEST_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/remoting/chromoting_host_info.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

namespace remoting {

// A class implements REST API insert call for the Chromoting directory service.
class DirectoryAddRequest : public URLFetcher::Delegate {
 public:
  enum Result {
    // Host was added successfully.
    SUCCESS,
    // Invalid authentication token.
    ERROR_AUTH,
    // Server rejected request because it was invalid (e.g. specified
    // public key is invalid).
    ERROR_INVALID_REQUEST,
    // Host is already registered.
    ERROR_EXISTS,
    // Internal server error.
    ERROR_SERVER,
    // Timeout expired.
    ERROR_TIMEOUT_EXPIRED,
    // Some other error, e.g. network failure.
    ERROR_OTHER,
  };

  // Callback called when request is finished. The second parameter
  // contains error message in case of an error. The error message may
  // not be localized, and should be used for logging, but not shown
  // to the user.
  typedef Callback2<Result, const std::string&>::Type DoneCallback;

  explicit DirectoryAddRequest(net::URLRequestContextGetter* getter);
  ~DirectoryAddRequest();

  // Add this computer as a host. Use the token for
  // authentication. |done_callback| is called when the request is
  // finished. Request can be cancelled by destroying this object.
  void AddHost(const remoting::ChromotingHostInfo& host_info,
               const std::string& auth_token,
               DoneCallback* done_callback);

  // URLFetcher::Delegate implementation.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

 private:
  friend class DirectoryAddRequestTest;

  net::URLRequestContextGetter* getter_;
  scoped_ptr<DoneCallback> done_callback_;
  scoped_ptr<URLFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryAddRequest);
};

}  // namespace remoting

#endif  // CHROME_BROWSER_REMOTING_DIRECTORY_ADD_REQUEST_H_
