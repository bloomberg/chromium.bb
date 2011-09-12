// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_CONTENT_UTILITY_CLIENT_H_
#define CONTENT_SHELL_SHELL_CONTENT_UTILITY_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/utility/content_utility_client.h"

namespace content {

  class ShellContentUtilityClient : public ContentUtilityClient {
 public:
  virtual ~ShellContentUtilityClient();
  virtual void UtilityThreadStarted() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_CONTENT_UTILITY_CLIENT_H_
