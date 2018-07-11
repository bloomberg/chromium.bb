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

  /* Indicates that the registration state of an object has changed.
   *
   * Arguments:
   *     client - the InvalidationClient invoking the listener
   *     object_id - the id of the object whose state changed
   *     reg_state - the new state
   */
  virtual void InformRegistrationStatus(InvalidationClient* client,
                                        const invalidation::ObjectId& object_id,
                                        RegistrationState reg_state) = 0;

  /* Indicates that an object registration or unregistration operation may have
   * failed.
   *
   * For transient failures, the application can retry the registration later -
   * if it chooses to do so, it must use a sensible backoff policy such as
   * exponential backoff. For permanent failures, it must not automatically
   * retry without fixing the situation (e.g., by presenting a dialog box to the
   * user).
   *
   * Arguments:
   *     client - the {@link InvalidationClient} invoking the listener
   *     object_id - the id of the object whose state changed
   *     is_transient - whether the error is transient or permanent
   *     errorMessage - extra information about the message
   */
  virtual void InformRegistrationFailure(
      InvalidationClient* client,
      const invalidation::ObjectId& object_id,
      bool is_transient,
      const std::string& error_message) = 0;

  /* Indicates that the all registrations for the client are in an unknown state
   * (e.g., they could have been removed). The application MUST inform the
   * InvalidationClient of its registrations once it receives this event.  The
   * requested objects are those for which the digest of their serialized object
   * ids matches a particular prefix bit-pattern. The digest for an object id is
   * computed as following (the digest chosen for this method is SHA-1):
   *
   *   Digest digest();
   *   digest.Update(Little endian encoding of object source type)
   *   digest.Update(object name)
   *   digest.GetDigestSummary()
   *
   * For a set of objects, digest is computed by sorting lexicographically based
   * on their digests and then performing the update process given above (i.e.,
   * calling digest.update on each object's digest and then calling
   * getDigestSummary at the end).
   *
   * IMPORTANT: A client can always register for more objects than what is
   * requested here. For example, in response to this call, the client can
   * ignore the prefix parameters and register for all its objects.
   *
   * Arguments:
   *     client - the InvalidationClient invoking the listener
   *     prefix - prefix of the object ids as described above.
   *     prefix_length - number of bits in prefix to consider.
   */
  virtual void ReissueRegistrations(InvalidationClient* client,
                                    const std::string& prefix,
                                    int prefix_length) = 0;

  /* Informs the listener about errors that have occurred in the backend, e.g.,
   * authentication, authorization problems.
   *
   * Arguments:
   *     client - the InvalidationClient invoking the listener
   *     error_info - information about the error
   */
  virtual void InformError(InvalidationClient* client,
                           const invalidation::ErrorInfo& error_info) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LISTENER_H_
