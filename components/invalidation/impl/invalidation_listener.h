// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LISTENER_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LISTENER_H_

#include <string>

namespace invalidation {
class ObjectId;
class Invalidation;
class ErrorInfo;
}  // namespace invalidation

namespace syncer {

class InvalidationClient;

// Handlers registration and message recieving events.
class InvalidationListener {
 public:
  /* Possible registration states for an object. */
  enum RegistrationState { REGISTERED, UNREGISTERED };

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
   */
  virtual void Invalidate(InvalidationClient* client,
                          const invalidation::Invalidation& invalidation) = 0;

  /* As Invalidate, but for an unknown application store version. The object may
   * or may not have been updated - to ensure that the application does not miss
   * an update from its backend, the application must check and/or fetch the
   * latest version from its store.
   */
  virtual void InvalidateUnknownVersion(
      InvalidationClient* client,
      const invalidation::ObjectId& object_id) = 0;

  /* Indicates that the application should consider all objects to have changed.
   * This event is generally sent when the client has been disconnected from the
   * network for too long a period and has been unable to resynchronize with the
   * update stream, but it may be invoked arbitrarily (although the service
   * tries hard not to invoke it under normal circumstances).
   *
   * Arguments:
   *     client - the InvalidationClient invoking the listener
   */
  virtual void InvalidateAll(InvalidationClient* client) = 0;

  /* Informs the listener about errors that have occurred in the backend, e.g.,
   * authentication, authorization problems.
   *
   * Arguments:
   *     client - the InvalidationClient invoking the listener
   *     error_info - information about the error
   */
  virtual void InformError(InvalidationClient* client,
                           const invalidation::ErrorInfo& error_info) = 0;

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
