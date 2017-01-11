// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_MOCK_RESOURCE_LOADER_H_
#define CONTENT_BROWSER_LOADER_MOCK_RESOURCE_LOADER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/loader/resource_controller.h"
#include "net/base/net_errors.h"

class GURL;

namespace net {
struct RedirectInfo;
class URLRequestStatus;
}

namespace content {
class ResourceHandler;
struct ResourceResponse;

// Class that takes the place of the ResourceLoader for tests. It simplifies
// testing ResourceHandlers by managing callbacks and performing basic sanity
// checks. The test fixture is responsible for advancing states.
class MockResourceLoader : public ResourceController {
 public:
  explicit MockResourceLoader(ResourceHandler* resource_handler);
  ~MockResourceLoader() override;

  // Idle means the ResourceHandler is waiting for the next call from the
  // TestFixture/ResourceLoader, CALLBACK_PENDING means that the ResourceHandler
  // will resume the request at some future point to resume the request, and
  // CANCELED means the ResourceHandler cancelled the request.
  enum class Status {
    // The MockLoader is waiting for more data from hte test fixture.
    IDLE,
    // The MockLoader is currently in the middle of a call to a handler. Will
    // switch to CALLBACK_PENDING if the handler defers handling the request.
    CALLING_HANDLER,
    // The MockLoader is waiting for a callback from the ResourceHandler.
    CALLBACK_PENDING,
    // The request was cancelled.
    CANCELED,
  };

  // These all run the corresponding methods on ResourceHandler, along with
  // basic sanity checks for the behavior of the handler. Each check returns the
  // current status of the ResourceLoader.
  Status OnWillStart(const GURL& url);
  Status OnRequestRedirected(const net::RedirectInfo& redirect_info,
                             scoped_refptr<ResourceResponse> response);
  Status OnResponseStarted(scoped_refptr<ResourceResponse> response);
  Status OnWillRead(int min_size);
  Status OnReadCompleted(int bytes_read);
  Status OnResponseCompleted(const net::URLRequestStatus& status);

  Status status() const { return status_; }

  // Network error passed to the first CancelWithError() / Cancel() call, which
  // is the one the real code uses in the case of multiple cancels.
  int error_code() const { return error_code_; }

 private:
  // ResourceController implementation.
  void Cancel() override;
  void CancelAndIgnore() override;
  void CancelWithError(int error_code) override;
  void Resume() override;

  ResourceHandler* const resource_handler_;

  Status status_ = Status::IDLE;
  int error_code_ = net::OK;

  DISALLOW_COPY_AND_ASSIGN(MockResourceLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_TEST_RESOURCE_HANDLER_WRAPPER_H_
