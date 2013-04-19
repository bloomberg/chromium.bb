// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_H_

#include <list>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"

class CommandLine;
class DevToolsEventLogger;

namespace base {
class DictionaryValue;
class FilePath;
}

class Chrome;
class Status;
class URLRequestContextGetter;

Status LaunchChrome(
    URLRequestContextGetter* context_getter,
    int port,
    const SyncWebSocketFactory& socket_factory,
    const Capabilities& capabilities,
    const std::list<DevToolsEventLogger*>& devtools_event_loggers,
    scoped_ptr<Chrome>* chrome);

namespace internal {
Status ProcessExtensions(const std::vector<std::string>& extensions,
                         const base::FilePath& temp_dir,
                         bool include_automation_extension,
                         CommandLine* command);
Status PrepareUserDataDir(
    const base::FilePath& user_data_dir,
    const base::DictionaryValue* custom_prefs,
    const base::DictionaryValue* custom_local_state);
}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_H_
