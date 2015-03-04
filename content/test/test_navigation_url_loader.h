// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_H_
#define CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/common/navigation_params.h"

namespace net {
struct RedirectInfo;
}

namespace content {

class NavigationURLLoaderDelegate;
class StreamHandle;
struct ResourceResponse;

// PlzNavigate
// Test implementation of NavigationURLLoader to simulate the network stack
// response.
class TestNavigationURLLoader
    : public NavigationURLLoader,
      public base::SupportsWeakPtr<TestNavigationURLLoader> {
 public:
  TestNavigationURLLoader(scoped_ptr<NavigationRequestInfo> request_info,
                          NavigationURLLoaderDelegate* delegate);

  // NavigationURLLoader implementation.
  void FollowRedirect() override;

  NavigationRequestInfo* request_info() const { return request_info_.get(); }

  void SimulateServerRedirect(const GURL& redirect_url);

  void CallOnRequestRedirected(const net::RedirectInfo& redirect_info,
                               const scoped_refptr<ResourceResponse>& response);
  void CallOnResponseStarted(const scoped_refptr<ResourceResponse>& response,
                             scoped_ptr<StreamHandle> body);

  int redirect_count() { return redirect_count_; }

 private:
  ~TestNavigationURLLoader() override;

  scoped_ptr<NavigationRequestInfo> request_info_;
  NavigationURLLoaderDelegate* delegate_;
  int redirect_count_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_H_
