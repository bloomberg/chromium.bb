// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple wrapper around invalidation::InvalidationClient that
// handles all the startup/shutdown details and hookups.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_CHROME_INVALIDATION_CLIENT_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_CHROME_INVALIDATION_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/non_thread_safe.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/notifier/chrome_system_resources.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "google/cacheinvalidation/invalidation-client.h"

namespace buzz {
class XmppClient;
}  // namespace

namespace sync_notifier {

class CacheInvalidationPacketHandler;
class RegistrationManager;

// TODO(akalin): Hook this up to a NetworkChangeNotifier so we can
// properly notify invalidation_client_.

class ChromeInvalidationClient : public invalidation::InvalidationListener {
 public:
  class Listener {
   public:
    virtual ~Listener();

    virtual void OnInvalidate(syncable::ModelType model_type) = 0;

    virtual void OnInvalidateAll() = 0;
  };

  ChromeInvalidationClient();

  // Calls Stop().
  virtual ~ChromeInvalidationClient();

  // Does not take ownership of |listener| nor |xmpp_client|.
  void Start(
      const std::string& client_id, Listener* listener,
      buzz::XmppClient* xmpp_client);

  void Stop();

  // Register the sync types that we're interested in getting
  // notifications for.  Must only be called between calls to Start()
  // and Stop().
  void RegisterTypes();

  // invalidation::InvalidationListener implementation.
  //
  // TODO(akalin): Move these into a private inner class.

  virtual void Invalidate(const invalidation::Invalidation& invalidation,
                          invalidation::Closure* callback);

  virtual void InvalidateAll(invalidation::Closure* callback);

  virtual void AllRegistrationsLost(invalidation::Closure* callback);

  virtual void RegistrationLost(const invalidation::ObjectId& object_id,
                                invalidation::Closure* callback);

 private:
  NonThreadSafe non_thread_safe_;
  ChromeSystemResources chrome_system_resources_;
  Listener* listener_;
  scoped_ptr<invalidation::InvalidationClient> invalidation_client_;
  scoped_ptr<CacheInvalidationPacketHandler>
      cache_invalidation_packet_handler_;
  scoped_ptr<RegistrationManager> registration_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeInvalidationClient);
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_CHROME_INVALIDATION_CLIENT_H_
