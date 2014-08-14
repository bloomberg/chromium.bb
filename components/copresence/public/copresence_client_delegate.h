// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CLIENT_DELEGATE_H_
#define COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CLIENT_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"

namespace net {
class URLRequestContextGetter;
}

namespace copresence {

class Message;
class WhispernetClient;

enum CopresenceStatus { SUCCESS, FAIL };

// Callback type to send a status.
typedef base::Callback<void(CopresenceStatus)> StatusCallback;

// A delegate interface for users of Copresence.
class CopresenceClientDelegate {
 public:
  // This method will be called when we have subscribed messages that need to
  // be sent to their respective apps.
  virtual void HandleMessages(
      const std::string& app_id,
      const std::string& subscription_id,
      const std::vector<Message>& message) = 0;

  virtual net::URLRequestContextGetter* GetRequestContext() const = 0;

  virtual const std::string GetPlatformVersionString() const = 0;

  virtual const std::string GetAPIKey() const = 0;

  virtual WhispernetClient* GetWhispernetClient() = 0;
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CLIENT_DELEGATE_H_
