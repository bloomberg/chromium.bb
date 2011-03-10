// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple wrapper around invalidation::InvalidationClient that
// handles all the startup/shutdown details and hookups.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_CHROME_INVALIDATION_CLIENT_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_CHROME_INVALIDATION_CLIENT_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_callback_factory.h"
#include "base/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/weak_ptr.h"
#include "chrome/browser/sync/notifier/chrome_system_resources.h"
#include "chrome/browser/sync/notifier/state_writer.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "google/cacheinvalidation/invalidation-client.h"

namespace talk_base {
class Task;
}  // namespace

namespace sync_notifier {

class CacheInvalidationPacketHandler;
class RegistrationManager;

// TODO(akalin): Hook this up to a NetworkChangeNotifier so we can
// properly notify invalidation_client_.

class ChromeInvalidationClient
    : public invalidation::InvalidationListener,
      public StateWriter {
 public:
  class Listener {
   public:
    virtual ~Listener();

    virtual void OnInvalidate(syncable::ModelType model_type,
                              const std::string& payload) = 0;

    virtual void OnInvalidateAll() = 0;
  };

  ChromeInvalidationClient();

  // Calls Stop().
  virtual ~ChromeInvalidationClient();

  // Does not take ownership of |listener| or |state_writer|.
  // |base_task| must still be non-NULL.
  void Start(
      const std::string& client_id, const std::string& client_info,
      const std::string& state, Listener* listener,
      StateWriter* state_writer, base::WeakPtr<talk_base::Task> base_task);

  void Stop();

  // Changes the task used to |base_task|, which must still be
  // non-NULL.  Must only be called between calls to Start() and
  // Stop().
  void ChangeBaseTask(base::WeakPtr<talk_base::Task> base_task);

  // Register the sync types that we're interested in getting
  // notifications for.  Must only be called between calls to Start()
  // and Stop().
  void RegisterTypes(const syncable::ModelTypeSet& types);

  // invalidation::InvalidationListener implementation.
  //
  // TODO(akalin): Move these into a private inner class.

  virtual void Invalidate(const invalidation::Invalidation& invalidation,
                          invalidation::Closure* callback);

  virtual void InvalidateAll(invalidation::Closure* callback);

  virtual void RegistrationStateChanged(
      const invalidation::ObjectId& object_id,
      invalidation::RegistrationState new_state,
      const invalidation::UnknownHint& unknown_hint);

  virtual void AllRegistrationsLost(invalidation::Closure* callback);

  // StateWriter implementation.
  virtual void WriteState(const std::string& state);

 private:
  friend class ChromeInvalidationClientTest;

  // Should only be called between calls to Start() and Stop().
  void HandleOutboundPacket(
      invalidation::NetworkEndpoint* const& network_endpoint);

  base::NonThreadSafe non_thread_safe_;
  ChromeSystemResources chrome_system_resources_;
  base::ScopedCallbackFactory<ChromeInvalidationClient>
      scoped_callback_factory_;
  scoped_ptr<invalidation::NetworkCallback> handle_outbound_packet_callback_;
  Listener* listener_;
  StateWriter* state_writer_;
  scoped_ptr<invalidation::InvalidationClient> invalidation_client_;
  scoped_ptr<CacheInvalidationPacketHandler>
      cache_invalidation_packet_handler_;
  scoped_ptr<RegistrationManager> registration_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeInvalidationClient);
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_CHROME_INVALIDATION_CLIENT_H_
