// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_FACTORY_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_FACTORY_H_

class CommandLine;

namespace sync_notifier {
class SyncNotifier;

// Class to instantiate various implementations of the SyncNotifier interface.
class SyncNotifierFactory {
 public:
  SyncNotifierFactory();
  ~SyncNotifierFactory();

  // Creates the appropriate sync notifier. The caller should take ownership
  // of the object returned and delete it when no longer used.
  SyncNotifier* CreateSyncNotifier(const CommandLine& command_line);
};
}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_SYNC_NOTIFIER_FACTORY_H_
