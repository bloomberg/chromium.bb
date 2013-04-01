// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process.h"
#include "chrome/test/chromedriver/chrome/chrome_impl.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"

class CommandLine;
class Status;
class URLRequestContextGetter;

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
}

class ChromeDesktopImpl : public ChromeImpl {
 public:
  ChromeDesktopImpl(URLRequestContextGetter* context_getter,
                    int port,
                    const SyncWebSocketFactory& socket_factory);
  virtual ~ChromeDesktopImpl();

  virtual Status Launch(const base::FilePath& exe,
                        const base::ListValue* args,
                        const base::ListValue* extensions,
                        const base::DictionaryValue* prefs,
                        const base::DictionaryValue* local_state,
                        const std::string& log_path);

  // Overriden from Chrome:
  virtual std::string GetOperatingSystemName() OVERRIDE;

  // Overridden from ChromeImpl:
  virtual Status Quit() OVERRIDE;

 private:
  base::ProcessHandle process_;
  base::ScopedTempDir user_data_dir_;
  base::ScopedTempDir extension_dir_;
};

namespace internal {
Status ProcessCommandLineArgs(const base::ListValue* args,
                              CommandLine* command);
Status ProcessExtensions(const base::ListValue* extensions,
                         const base::FilePath& temp_dir,
                         CommandLine* command);
Status PrepareUserDataDir(
    const base::FilePath& user_data_dir,
    const base::DictionaryValue* custom_prefs,
    const base::DictionaryValue* custom_local_state);
}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_
