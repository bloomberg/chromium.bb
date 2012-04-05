// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

void ProcessSingleton::Unlock() {
    DCHECK(CalledOnValidThread());
    locked_ = false;
    foreground_window_ = NULL;
    // Replay the command lines of the messages which were received while the
    // ProcessSingleton was locked. Only replay each message once.
    std::set<DelayedStartupMessage> replayed_messages;
    for (std::vector<DelayedStartupMessage>::const_iterator it =
             saved_startup_messages_.begin();
         it != saved_startup_messages_.end(); ++it) {
      if (replayed_messages.find(*it) !=
          replayed_messages.end())
        continue;
      notification_callback_.Run(CommandLine(it->first), it->second);
      replayed_messages.insert(*it);
    }
    saved_startup_messages_.clear();
}
