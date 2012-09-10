// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_SERVICE_H
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_SERVICE_H

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/shared_memory.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"

namespace content {

class GamepadDataFetcher;
class GamepadProvider;
class GamepadServiceTestConstructor;
class RenderProcessHost;

// Owns the GamepadProvider (the background polling thread) and keeps track of
// the number of consumers currently using the data (and pausing the provider
// when not in use).
class CONTENT_EXPORT GamepadService {
 public:
  // Returns the GamepadService singleton.
  static GamepadService* GetInstance();

  // Increments the number of users of the provider. The Provider is running
  // when there's > 0 users, and is paused when the count drops to 0.
  //
  // Must be called on the I/O thread.
  void AddConsumer();

  // Removes a consumer. Should be matched with an AddConsumer call.
  //
  // Must be called on the I/O thread.
  void RemoveConsumer();

  // Registers the given closure for calling when the user has interacted with
  // the device. This callback will only be issued once. Should only be called
  // while a consumer is active.
  void RegisterForUserGesture(const base::Closure& closure);

  // Returns the shared memory handle of the gamepad data duplicated into the
  // given process.
  base::SharedMemoryHandle GetSharedMemoryHandleForProcess(
      base::ProcessHandle handle);

  // Stop/join with the background thread in GamepadProvider |provider_|.
  void Terminate();

 private:
  friend struct DefaultSingletonTraits<GamepadService>;
  friend class GamepadServiceTestConstructor;

  GamepadService();

  // Constructor for testing. This specifies the data fetcher to use for a
  // provider, bypassing the default platform one.
  GamepadService(scoped_ptr<GamepadDataFetcher> fetcher);

  virtual ~GamepadService();

  int num_readers_;
  scoped_ptr<GamepadProvider> provider_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(GamepadService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_SERVICE_H_
