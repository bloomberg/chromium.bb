// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_NET_LOG_OBSERVER_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_NET_LOG_OBSERVER_H_

#include <memory>

#include "base/macros.h"

namespace base {
class FilePath;
}  // namespace base

namespace net {
class FileNetLogObserver;
}  // namespace net

class WebEngineNetLogObserver {
 public:
  explicit WebEngineNetLogObserver(const base::FilePath& log_path);
  ~WebEngineNetLogObserver();

 private:
  std::unique_ptr<net::FileNetLogObserver> file_net_log_observer_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineNetLogObserver);
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_NET_LOG_OBSERVER_H_
