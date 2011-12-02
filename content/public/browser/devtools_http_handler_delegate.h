// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_DELEGATE_H_
#pragma once

#include <string>
#include <vector>

class TabContents;

namespace net {
class URLRequestContext;
}

namespace content {

class DevToolsHttpHandlerDelegate {
 public:
  typedef std::vector<TabContents*> InspectableTabs;
  virtual ~DevToolsHttpHandlerDelegate() {}

  // Should return the list of inspectable tabs. Called on the UI thread.
  virtual InspectableTabs GetInspectableTabs() = 0;

  // Should return discovery page HTML that should list available tabs
  // and provide attach links. Called on the IO thread.
  virtual std::string GetDiscoveryPageHTML() = 0;

  // Should return URL request context for issuing requests against devtools
  // webui or NULL if no context is available. Called on the IO thread.
  virtual net::URLRequestContext* GetURLRequestContext() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_DELEGATE_H_
