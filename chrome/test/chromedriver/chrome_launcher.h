// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"

class CommandLine;

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
}

class Chrome;
class Status;
class URLRequestContextGetter;

Status LaunchDesktopChrome(URLRequestContextGetter* context_getter,
                           int port,
                           const SyncWebSocketFactory& socket_factory,
                           const base::FilePath& exe,
                           const base::ListValue* args,
                           const base::ListValue* extensions,
                           const base::DictionaryValue* prefs,
                           const base::DictionaryValue* local_state,
                           const std::string& log_path,
                           scoped_ptr<Chrome>* chrome);

Status LaunchAndroidChrome(URLRequestContextGetter* context_getter,
                           int port,
                           const SyncWebSocketFactory& socket_factory,
                           const std::string& package_name,
                           scoped_ptr<Chrome>* chrome);

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

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_H_
