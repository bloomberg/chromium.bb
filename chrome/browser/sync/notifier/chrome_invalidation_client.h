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
#include "google/cacheinvalidation/invalidation-client.h"

namespace buzz {
class XmppClient;
}  // namespace

namespace sync_notifier {

class CacheInvalidationPacketHandler;

// TODO(akalin): Hook this up to a NetworkChangeNotifier so we can
// properly notify invalidation_client_.

class ChromeInvalidationClient {
 public:
  ChromeInvalidationClient();

  ~ChromeInvalidationClient();

  // Does not take ownership of |listener| nor |xmpp_client|.
  void Start(
      const std::string& app_name,
      invalidation::InvalidationListener* listener,
      buzz::XmppClient* xmpp_client);

  void Stop();

  // The following functions must only be called between calls to
  // Start() and Stop().  See invalidation-client.h for documentation.

  void Register(const invalidation::ObjectId& oid,
                invalidation::RegistrationCallback* callback);

  void Unregister(const invalidation::ObjectId& oid,
                  invalidation::RegistrationCallback* callback);

 private:
  NonThreadSafe non_thread_safe_;
  ChromeSystemResources chrome_system_resources_;
  scoped_ptr<invalidation::InvalidationClient> invalidation_client_;
  scoped_ptr<CacheInvalidationPacketHandler>
      cache_invalidation_packet_handler_;

  DISALLOW_COPY_AND_ASSIGN(ChromeInvalidationClient);
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_CHROME_INVALIDATION_CLIENT_H_
