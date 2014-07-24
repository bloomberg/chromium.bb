// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_INVALIDATION_H_
#define COMPONENTS_INVALIDATION_INVALIDATION_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/invalidation/ack_handle.h"
#include "components/invalidation/invalidation_export.h"
#include "google/cacheinvalidation/include/types.h"
#include "sync/internal_api/public/util/weak_handle.h"

namespace syncer {

class DroppedInvalidationTracker;
class AckHandler;

// Represents a local invalidation, and is roughly analogous to
// invalidation::Invalidation.  Unlike invalidation::Invalidation, this class
// supports "local" ack-tracking and simple serialization to pref values.
class INVALIDATION_EXPORT Invalidation {
 public:
  // Factory functions.
  static Invalidation Init(const invalidation::ObjectId& id,
                           int64 version,
                           const std::string& payload);
  static Invalidation InitUnknownVersion(const invalidation::ObjectId& id);
  static Invalidation InitFromDroppedInvalidation(const Invalidation& dropped);
  static scoped_ptr<Invalidation> InitFromValue(
      const base::DictionaryValue& value);

  ~Invalidation();

  // Compares two invalidations.  The comparison ignores ack-tracking state.
  bool Equals(const Invalidation& other) const;

  invalidation::ObjectId object_id() const;
  bool is_unknown_version() const;

  // Safe to call only if is_unknown_version() returns false.
  int64 version() const;

  // Safe to call only if is_unknown_version() returns false.
  const std::string& payload() const;

  const AckHandle& ack_handle() const;

  // Sets the AckHandler to be used to track this Invalidation.
  //
  // This should be set by the class that generates the invalidation.  Clients
  // of the Invalidations API should not need to call this.
  //
  // Note that some sources of invalidations do not support ack tracking, and do
  // not set the ack_handler.  This will be hidden from users of this class.
  void set_ack_handler(syncer::WeakHandle<AckHandler> ack_handler);

  // Returns whether or not this instance supports ack tracking.  This will
  // depend on whether or not the source of invaliadations supports
  // invalidations.
  //
  // Clients can safely ignore this flag.  They can assume that all
  // invalidations support ack tracking.  If they're wrong, then invalidations
  // will be less reliable, but their behavior will be no less correct.
  bool SupportsAcknowledgement() const;

  // Acknowledges the receipt of this invalidation.
  //
  // Clients should call this on a received invalidation when they have fully
  // processed the invalidation and persisted the results to disk.  Once this
  // function is called, the invalidations system is under no obligation to
  // re-deliver this invalidation in the event of a crash or restart.
  void Acknowledge() const;

  // Informs the ack tracker that this invalidation will not be serviced.
  //
  // If a client's buffer reaches its limit and it is forced to start dropping
  // invalidations, it should call this function before dropping its
  // invalidations in order to allow the ack tracker to drop the invalidation,
  // too.
  //
  // To indicate recovery from a drop event, the client should call
  // Acknowledge() on the most recently dropped inavlidation.
  void Drop();

  scoped_ptr<base::DictionaryValue> ToValue() const;
  std::string ToString() const;

 private:
  Invalidation(const invalidation::ObjectId& id,
               bool is_unknown_version,
               int64 version,
               const std::string& payload,
               AckHandle ack_handle);

  // The ObjectId to which this invalidation belongs.
  invalidation::ObjectId id_;

  // This flag is set to true if this is an unknown version invalidation.
  bool is_unknown_version_;

  // The version number of this invalidation.  Should not be accessed if this is
  // an unkown version invalidation.
  int64 version_;

  // The payaload associated with this invalidation.  Should not be accessed if
  // this is an unknown version invalidation.
  std::string payload_;

  // A locally generated unique ID used to manage local acknowledgements.
  AckHandle ack_handle_;
  syncer::WeakHandle<AckHandler> ack_handler_;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_INVALIDATION_H_
