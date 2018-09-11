// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LISTENER_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LISTENER_H_

#include <string>

namespace syncer {

class InvalidationClient;

// Handlers registration and message recieving events.
class InvalidationListener {
 public:
  virtual ~InvalidationListener() {}

  /* Called in response to the InvalidationClient::Start call. Indicates that
   * the InvalidationClient is now ready for use, i.e., calls such as
   * register/unregister can be performed on that object.
   *
   * Arguments:
   *     client - the InvalidationClient invoking the listener
   */
  virtual void Ready(InvalidationClient* client) = 0;

  /* Indicates that an object has been updated to a particular version.
   *
   * The Ticl guarantees that this callback will be invoked at least once for
   * every invalidation that it guaranteed to deliver. It does not guarantee
   * exactly-once delivery or in-order delivery (with respect to the version
   * number).
   *
   * Arguments:
   *     client - the InvalidationClient invoking the listener
   *     payload - additional info specific to the invalidations
   *     version - version of the invalidation
   *     private_topic_name - the topic, which was invalidated
   */
  virtual void Invalidate(InvalidationClient* client,
                          const std::string& payload,
                          const std::string& private_topic,
                          const std::string& public_topic,
                          int64_t version) = 0;

  /* Informs the listener token
   *
   * Arguments:
   *     client - the InvalidationClient invoking the listener
   *     token - instance id token, recieved from network
   */
  virtual void InformTokenRecieved(InvalidationClient* client,
                                   const std::string& token) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LISTENER_H_
