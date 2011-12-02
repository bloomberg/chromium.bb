// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Owns the GamepadProvider (the background polling thread) and keeps track of
// the number of renderers currently using the data (and pausing the provider
// when not in use).

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_SERVICE_H
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_SERVICE_H

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/shared_memory.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {

class GamepadDataFetcher;
class GamepadProvider;
class RenderProcessHost;

class GamepadService : public NotificationObserver {
 public:
  // Returns the GamepadService singleton.
  static GamepadService* GetInstance();

  // Called on IO thread from a renderer host. Increments the number of users
  // of the provider. The Provider is running when there's > 0 users, and is
  // paused when the count drops to 0. There is no stop, the gamepad service
  // registers with the RPH to be notified when the associated renderer closes
  // (or crashes).
  void Start(GamepadDataFetcher* fetcher,
             RenderProcessHost* associated_rph);

  base::SharedMemoryHandle GetSharedMemoryHandle(base::ProcessHandle handle);

 private:
  friend struct DefaultSingletonTraits<GamepadService>;
  friend class base::RefCountedThreadSafe<GamepadService>;
  GamepadService();
  virtual ~GamepadService();

  // Called when a renderer that Start'd us is closed/crashes.
  void Stop();

  // Run on UI thread to receive notifications of renderer closes.
  void RegisterForCloseNotification(RenderProcessHost* rph);

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // A registrar for listening notifications. Used to listen for when an
  // associated renderer has gone away (possibly crashed). We don't trust
  // the renderers to send a stop message because of the possibility of
  // crashing.
  NotificationRegistrar registrar_;

  int num_readers_;
  scoped_refptr<GamepadProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(GamepadService);
};

} // namespace content

#endif // CONTENT_BROWSER_GAMEPAD_GAMEPAD_SERVICE_H
