// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SESSION_ASSIGNMENT_SOURCE_H_
#define BLIMP_CLIENT_SESSION_ASSIGNMENT_SOURCE_H_

#include <string>

#include "base/callback.h"
#include "blimp/client/blimp_client_export.h"
#include "net/base/ip_endpoint.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace blimp {
namespace client {

// TODO(kmarshall): Take values from configuration data.
const char kDummyClientToken[] = "MyVoiceIsMyPassport";

// Potential assigner URLs.
const char kDefaultAssignerURL[] =
    "https://blimp-pa.googleapis.com/v1/assignment";

// An Assignment contains the configuration data needed for a client
// to connect to the engine.
struct BLIMP_CLIENT_EXPORT Assignment {
  enum TransportProtocol {
    UNKNOWN = 0,
    SSL = 1,
    TCP = 2,
    QUIC = 3,
  };

  Assignment();
  ~Assignment();

  TransportProtocol transport_protocol;
  net::IPEndPoint ip_endpoint;
  std::string client_token;
  std::string certificate;
  std::string certificate_fingerprint;

  // Returns true if the net::IPEndPoint has an unspecified IP, port, or
  // transport protocol.
  bool is_null() const;
};

// AssignmentSource provides functionality to find out how a client should
// connect to an engine.
class BLIMP_CLIENT_EXPORT AssignmentSource : public net::URLFetcherDelegate {
 public:
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.blimp.assignment
  enum Result {
    RESULT_UNKNOWN = 0,
    RESULT_OK = 1,
    RESULT_BAD_REQUEST = 2,
    RESULT_BAD_RESPONSE = 3,
    RESULT_INVALID_PROTOCOL_VERSION = 4,
    RESULT_EXPIRED_ACCESS_TOKEN = 5,
    RESULT_USER_INVALID = 6,
    RESULT_OUT_OF_VMS = 7,
    RESULT_SERVER_ERROR = 8,
    RESULT_SERVER_INTERRUPTED = 9,
    RESULT_NETWORK_FAILURE = 10
  };

  typedef base::Callback<void(AssignmentSource::Result, const Assignment&)>
      AssignmentCallback;

  // The |main_task_runner| should be the task runner for the UI thread because
  // this will in some cases be used to trigger user interaction on the UI
  // thread.
  AssignmentSource(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~AssignmentSource() override;

  // Retrieves a valid assignment for the client and posts the result to the
  // given callback.  |client_auth_token| is the OAuth2 access token to send to
  // the assigner when requesting an assignment.  If this is called before a
  // previous call has completed, the old callback will be called with
  // RESULT_SERVER_INTERRUPTED and no Assignment.
  void GetAssignment(const std::string& client_auth_token,
                     const AssignmentCallback& callback);

  // net::URLFetcherDelegate implementation:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 private:
  void ParseAssignerResponse();

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  scoped_ptr<net::URLFetcher> url_fetcher_;

  // This callback is set during a call to GetAssignment() and is cleared after
  // the request has completed (whether it be a success or failure).
  AssignmentCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(AssignmentSource);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_SESSION_ASSIGNMENT_SOURCE_H_
