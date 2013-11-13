// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_LOG_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_LOG_PRIVATE_API_H_

#include <string>

#include "chrome/browser/chromeos/system_logs/about_system_logs_fetcher.h"
#include "chrome/browser/extensions/api/log_private/filter_handler.h"
#include "chrome/browser/extensions/api/log_private/log_parser.h"
#include "chrome/common/extensions/api/log_private.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class LogPrivateGetHistoricalFunction : public AsyncExtensionFunction {
 public:
  LogPrivateGetHistoricalFunction();
  DECLARE_EXTENSION_FUNCTION("logPrivate.getHistorical",
                             LOGPRIVATE_GETHISTORICAL);

 protected:
  virtual ~LogPrivateGetHistoricalFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnSystemLogsLoaded(scoped_ptr<chromeos::SystemLogsResponse> sys_info);

  scoped_ptr<FilterHandler> filter_handler_;

  DISALLOW_COPY_AND_ASSIGN(LogPrivateGetHistoricalFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_LOG_PRIVATE_API_H_
