// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DESKTOP_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DESKTOP_IMPL_H_

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process.h"
#include "chrome/test/chromedriver/chrome_impl.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"

class CommandLine;
class Status;
class URLRequestContextGetter;

namespace base {
class FilePath;
class ListValue;
}

class ChromeDesktopImpl : public ChromeImpl {
 public:
  ChromeDesktopImpl(URLRequestContextGetter* context_getter,
                    int port,
                    const SyncWebSocketFactory& socket_factory);
  virtual ~ChromeDesktopImpl();

  virtual Status Launch(const base::FilePath& chrome_exe,
                        const base::ListValue* chrome_args);

  // Overridden from ChromeImpl:
  virtual Status Quit() OVERRIDE;

 private:
  base::ProcessHandle process_;
  base::ScopedTempDir user_data_dir_;
};

namespace internal {
Status ProcessCommandLineArgs(const base::ListValue* args,
                              CommandLine* command);
}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DESKTOP_IMPL_H_
