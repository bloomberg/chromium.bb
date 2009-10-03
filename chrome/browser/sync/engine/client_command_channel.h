// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_CLIENT_COMMAND_CHANNEL_H_
#define CHROME_BROWSER_SYNC_ENGINE_CLIENT_COMMAND_CHANNEL_H_

#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/util/event_sys.h"

namespace browser_sync {

// Commands for the client come back in sync responses, which is kind of
// inconvenient because some services (like the bandwidth throttler) want to
// know about them. So to avoid explicit dependencies on this protocol
// behavior, the syncer dumps all client commands onto a shared client command
// channel.

struct ClientCommandChannelTraits {
  typedef const sync_pb::ClientCommand* EventType;
  static inline bool IsChannelShutdownEvent(const EventType &event) {
    return 0 == event;
  }
};

typedef EventChannel<ClientCommandChannelTraits, Lock>
    ClientCommandChannel;

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_CLIENT_COMMAND_CHANNEL_H_
