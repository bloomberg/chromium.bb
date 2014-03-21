// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROTOCOL_EVENT_OBSERVER_H_
#define CHROME_BROWSER_SYNC_PROTOCOL_EVENT_OBSERVER_H_

namespace syncer {
class ProtocolEvent;
}

namespace browser_sync {

class ProtocolEventObserver {
 public:
  ProtocolEventObserver();
  virtual ~ProtocolEventObserver();

  virtual void OnProtocolEvent(const syncer::ProtocolEvent& event) = 0;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_PROTOCOL_EVENT_OBSERVER_H_
