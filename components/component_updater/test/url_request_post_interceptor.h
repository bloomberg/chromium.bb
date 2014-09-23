// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_TEST_URL_REQUEST_POST_INTERCEPTOR_H_
#define COMPONENTS_COMPONENT_UPDATER_TEST_URL_REQUEST_POST_INTERCEPTOR_H_

#include <stdint.h>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace net {
class URLRequest;
}

namespace component_updater {

// component 1 has extension id "jebgalgnebhfojomionfpkfelancnnkf", and
// the RSA public key the following hash:
const uint8_t jebg_hash[] = {0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec,
                             0x8e, 0xd5, 0xfa, 0x54, 0xb0, 0xd2, 0xdd, 0xa5,
                             0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47, 0xf6, 0xc4,
                             0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};
// component 2 has extension id "abagagagagagagagagagagagagagagag", and
// the RSA public key the following hash:
const uint8_t abag_hash[] = {0x01, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
                             0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
                             0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
                             0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x01};
// component 3 has extension id "ihfokbkgjpifnbbojhneepfflplebdkc", and
// the RSA public key the following hash:
const uint8_t ihfo_hash[] = {0x87, 0x5e, 0xa1, 0xa6, 0x9f, 0x85, 0xd1, 0x1e,
                             0x97, 0xd4, 0x4f, 0x55, 0xbf, 0xb4, 0x13, 0xa2,
                             0xe7, 0xc5, 0xc8, 0xf5, 0x60, 0x19, 0x78, 0x1b,
                             0x6d, 0xe9, 0x4c, 0xeb, 0x96, 0x05, 0x42, 0x17};

// Intercepts requests to a file path, counts them, and captures the body of
// the requests. Optionally, for each request, it can return a canned response
// from a given file. The class maintains a queue of expectations, and returns
// one and only one response for each request that matches and it is
// intercepted.
class URLRequestPostInterceptor {
 public:
  // Allows a generic string maching interface when setting up expectations.
  class RequestMatcher {
   public:
    virtual bool Match(const std::string& actual) const = 0;
    virtual ~RequestMatcher() {}
  };

  // Returns the url that is intercepted.
  GURL GetUrl() const;

  // Sets an expection for the body of the POST request and optionally,
  // provides a canned response identified by a |file_path| to be returned when
  // the expectation is met. If no |file_path| is provided, then an empty
  // response body is served. If |response_code| is provided, then an empty
  // response body with that response code is returned.
  // Returns |true| if the expectation was set. This class takes ownership of
  // the |request_matcher| object.
  bool ExpectRequest(class RequestMatcher* request_matcher);
  bool ExpectRequest(class RequestMatcher* request_matcher, int response_code);
  bool ExpectRequest(class RequestMatcher* request_matcher,
                     const base::FilePath& filepath);

  // Returns how many requests have been intercepted and matched by
  // an expectation. One expectation can only be matched by one request.
  int GetHitCount() const;

  // Returns how many requests in total have been captured by the interceptor.
  int GetCount() const;

  // Returns all requests that have been intercepted, matched or not.
  std::vector<std::string> GetRequests() const;

  // Returns all requests as a string for debugging purposes.
  std::string GetRequestsAsString() const;

  // Resets the state of the interceptor so that new expectations can be set.
  void Reset();

  class Delegate;

 private:
  friend class URLRequestPostInterceptorFactory;

  static const int kResponseCode200 = 200;

  struct ExpectationResponse {
    ExpectationResponse(int code, const std::string& body)
        : response_code(code), response_body(body) {}
    const int response_code;
    const std::string response_body;
  };
  typedef std::pair<const RequestMatcher*, ExpectationResponse> Expectation;

  URLRequestPostInterceptor(
      const GURL& url,
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);
  ~URLRequestPostInterceptor();

  void ClearExpectations();

  const GURL url_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  mutable base::Lock interceptor_lock_;
  mutable int hit_count_;
  mutable std::vector<std::string> requests_;
  mutable std::queue<Expectation> expectations_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestPostInterceptor);
};

class URLRequestPostInterceptorFactory {
 public:
  URLRequestPostInterceptorFactory(
      const std::string& scheme,
      const std::string& hostname,
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);
  ~URLRequestPostInterceptorFactory();

  // Creates an interceptor object for the specified url path. Returns NULL
  // in case of errors or a valid interceptor object otherwise. The caller
  // does not own the returned object.
  URLRequestPostInterceptor* CreateInterceptor(const base::FilePath& filepath);

 private:
  const std::string scheme_;
  const std::string hostname_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // After creation, |delegate_| lives on the IO thread and it is owned by
  // a URLRequestFilter after registration. A task to unregister it and
  // implicitly destroy it is posted from ~URLRequestPostInterceptorFactory().
  URLRequestPostInterceptor::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestPostInterceptorFactory);
};

// Intercepts HTTP POST requests sent to "localhost2".
class InterceptorFactory : public URLRequestPostInterceptorFactory {
 public:
  explicit InterceptorFactory(
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);
  ~InterceptorFactory();

  // Creates an interceptor for the url path defined by POST_INTERCEPT_PATH.
  URLRequestPostInterceptor* CreateInterceptor();

  // Creates an interceptor for the given url path.
  URLRequestPostInterceptor* CreateInterceptorForPath(const char* url_path);

 private:
  DISALLOW_COPY_AND_ASSIGN(InterceptorFactory);
};

class PartialMatch : public URLRequestPostInterceptor::RequestMatcher {
 public:
  explicit PartialMatch(const std::string& expected) : expected_(expected) {}
  virtual bool Match(const std::string& actual) const OVERRIDE;

 private:
  const std::string expected_;

  DISALLOW_COPY_AND_ASSIGN(PartialMatch);
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_TEST_URL_REQUEST_POST_INTERCEPTOR_H_
