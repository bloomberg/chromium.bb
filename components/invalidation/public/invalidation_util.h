// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Various utilities for dealing with invalidation data types.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_UTIL_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_UTIL_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/values.h"
#include "components/invalidation/public/invalidation_export.h"

namespace syncer {

// Used by UMA histogram, so entries shouldn't be reordered or removed.
enum class HandlerOwnerType {
  kCloud = 0,
  kFake = 1,
  kRemoteCommands = 2,
  kDrive = 3,
  kSync = 4,
  kTicl = 5,
  kChildAccount = 6,
  kNotificationPrinter = 7,
  kInvalidatorShim = 8,
  kSyncEngineImpl = 9,
  kUnknown = 10,
  kMaxValue = kUnknown,
};

class Invalidation;

struct INVALIDATION_EXPORT InvalidationVersionLessThan {
  bool operator()(const Invalidation& a, const Invalidation& b) const;
};

using Topic = std::string;
// It should be std::set, since std::set_difference is used for it.
using TopicSet = std::set<std::string>;

using TopicCountMap = std::map<Topic, int>;

INVALIDATION_EXPORT struct TopicMetadata {
  // Whether the topic is public.
  bool is_public;
};

INVALIDATION_EXPORT bool operator==(const TopicMetadata&, const TopicMetadata&);

using Topics = std::map<std::string, TopicMetadata>;

HandlerOwnerType OwnerNameToHandlerType(const std::string& owner_name);

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_UTIL_H_
