// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_RECEIVER_LIST_H_
#define API_IMPL_RECEIVER_LIST_H_

#include <vector>

#include "api/public/service_info.h"

namespace openscreen {

class ReceiverList {
 public:
  ReceiverList();
  ~ReceiverList();
  ReceiverList(ReceiverList&&) = delete;
  ReceiverList& operator=(ReceiverList&&) = delete;

  void OnReceiverAdded(const ServiceInfo& info);

  // Returns true if |info.service_id| matched an item in |receivers_| and was
  // therefore changed, otherwise false.
  bool OnReceiverChanged(const ServiceInfo& info);

  // Returns true if an item matching |info| was removed from |receivers_|,
  // otherwise false.
  bool OnReceiverRemoved(const ServiceInfo& info);

  // Returns true if |receivers_| was not empty before this call, otherwise
  // false.
  bool OnAllReceiversRemoved();

  const std::vector<ServiceInfo>& receivers() const { return receivers_; }

 private:
  std::vector<ServiceInfo> receivers_;
};

}  // namespace openscreen

#endif  // API_IMPL_RECEIVER_LIST_H_
