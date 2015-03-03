// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_DATA_REDUCTION_PROXY_MESSAGE_FILTER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_DATA_REDUCTION_PROXY_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"

namespace net {
class HostPortPair;
}

namespace data_reduction_proxy {

class DataReductionProxyConfig;
class DataReductionProxySettings;

// An IPC listener to handle DataReductionProxy IPC messages from the renderer.
class DataReductionProxyMessageFilter
    : public content::BrowserMessageFilter {
 public:
  DataReductionProxyMessageFilter(DataReductionProxySettings* settings);

  // Sets |response| to true if the |proxy_server| corresponds to a Data
  // Reduction Proxy.
  void OnIsDataReductionProxy(const net::HostPortPair& proxy_server,
                              bool* response);

 private:
  ~DataReductionProxyMessageFilter() override;

  // BrowserMessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;

  // Must outlive |this|.
  DataReductionProxyConfig* config_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMessageFilter);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_DATA_REDUCTION_PROXY_MESSAGE_FILTER_H_
