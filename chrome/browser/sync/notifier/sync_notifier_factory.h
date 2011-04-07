// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_FACTORY_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_FACTORY_H_

#include <string>

#include "base/memory/ref_counted.h"

class CommandLine;

namespace net {
class URLRequestContextGetter;
}

namespace sync_notifier {

class SyncNotifier;

// Class to instantiate various implementations of the SyncNotifier interface.
class SyncNotifierFactory {
 public:
  // |client_info| is a string identifying the client, e.g. a user
  // agent string.
  explicit SyncNotifierFactory(const std::string& client_info);
  ~SyncNotifierFactory();

  // Creates the appropriate sync notifier. The caller should take ownership
  // of the object returned and delete it when no longer used.
  SyncNotifier* CreateSyncNotifier(
      const CommandLine& command_line,
      const scoped_refptr<net::URLRequestContextGetter>&
          request_context_getter);

 private:
  const std::string client_info_;
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_FACTORY_H_
