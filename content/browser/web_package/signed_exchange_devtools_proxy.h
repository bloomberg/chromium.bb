// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_DEVTOOLS_PROXY_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_DEVTOOLS_PROXY_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

// SignedExchangeDevToolsProxy lives on the IO thread and sends messages to
// DevTools via the UI thread to show signed exchange related information.
// Currently it is used only to send error messages.
class CONTENT_EXPORT SignedExchangeDevToolsProxy {
 public:
  // |frame_tree_node_id_getter| callback will be called on the UI thread to get
  // the frame tree node ID. Note: We are using callback beause when Network
  // Service is not enabled the ID is not available while handling prefetch
  // requests on the IO thread. TODO(crbug/830506): Stop using callback after
  // shipping Network Service.
  explicit SignedExchangeDevToolsProxy(
      base::RepeatingCallback<int(void)> frame_tree_node_id_getter);
  ~SignedExchangeDevToolsProxy();

  void ReportErrorMessage(const std::string& message);

 private:
  const base::RepeatingCallback<int(void)> frame_tree_node_id_getter_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeDevToolsProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_DEVTOOLS_PROXY_H_
