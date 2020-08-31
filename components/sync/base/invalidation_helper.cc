// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/invalidation_helper.h"

#include <string>

namespace syncer {

TopicSet ModelTypeSetToTopicSet(ModelTypeSet model_types) {
  TopicSet topics;
  for (ModelType type : model_types) {
    Topic topic;
    if (!RealModelTypeToNotificationType(type, &topic)) {
      DLOG(WARNING) << "Invalid model type " << type;
      continue;
    }
    topics.insert(topic);
  }
  return topics;
}

}  // namespace syncer
