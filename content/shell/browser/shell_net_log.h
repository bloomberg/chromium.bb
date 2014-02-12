// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_NET_LOG_H_
#define CONTENT_SHELL_BROWSER_SHELL_NET_LOG_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "net/base/net_log_logger.h"

namespace content {

class ShellNetLog : public net::NetLog {
 public:
  explicit ShellNetLog(const std::string& app_name);
  virtual ~ShellNetLog();

 private:
  scoped_ptr<net::NetLogLogger> net_log_logger_;

  DISALLOW_COPY_AND_ASSIGN(ShellNetLog);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_NET_LOG_H_
