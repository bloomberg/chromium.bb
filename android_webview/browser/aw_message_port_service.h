// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_MESSAGE_PORT_SERVICE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_MESSAGE_PORT_SERVICE_H_

#include <vector>

#include "base/values.h"

namespace android_webview {

class AwMessagePortMessageFilter;

// The interface for AwMessagePortService
class AwMessagePortService  {
 public:
  virtual ~AwMessagePortService() { }

  virtual void OnConvertedWebToAppMessage(
      int message_port_id,
      const base::ListValue& message,
      const std::vector<int>& sent_message_port_ids) = 0;

  virtual void OnMessagePortMessageFilterClosing(
      AwMessagePortMessageFilter* filter) = 0;

  virtual void CleanupPort(int message_port_id) = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_MESSAGE_PORT_SERVICE_H_
