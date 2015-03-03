// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NAVIGATOR_CONNECT_SERVICE_FACTORY_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NAVIGATOR_CONNECT_SERVICE_FACTORY_H_

#include <map>
#include <string>

#include "content/public/browser/navigator_connect_service_factory.h"

namespace content {

// Implementation of NavigatorConnectServiceFactory that provides services
// layout tests can connect to under the chrome-layout-test: schema.
// In particular this implements an 'echo' service, that just sends back any
// message sends to it, and an 'annotate' service, which sends back some extra
// metadata in addition to the message it received.
class LayoutTestNavigatorConnectServiceFactory
    : public NavigatorConnectServiceFactory {
 public:
  LayoutTestNavigatorConnectServiceFactory();
  ~LayoutTestNavigatorConnectServiceFactory() override;

  // NavigatorConnectServiceFactory implementation.
  bool HandlesUrl(const GURL& target_url) override;
  void Connect(const NavigatorConnectClient& client,
               const ConnectCallback& callback) override;

 private:
  // |service_| is created and destroyed on the IO thread, while
  // LayoutTestNavigatorConnectServiceFactory can be destroyed on any thread.
  class Service;
  Service* service_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NAVIGATOR_CONNECT_SERVICE_FACTORY_H_
