// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/can_make_payment_query.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "url/origin.h"

namespace payments {

CanMakePaymentQuery::CanMakePaymentQuery() {}

CanMakePaymentQuery::~CanMakePaymentQuery() {}

bool CanMakePaymentQuery::CanQuery(
    const url::Origin& top_level_origin,
    const url::Origin& frame_origin,
    const std::map<std::string, std::set<std::string>>& query) {
  const std::string id =
      frame_origin.Serialize() + ":" + top_level_origin.Serialize();

  const auto& it = queries_.find(id);
  if (it == queries_.end()) {
    std::unique_ptr<base::OneShotTimer> timer =
        base::MakeUnique<base::OneShotTimer>();
    timer->Start(FROM_HERE, base::TimeDelta::FromMinutes(30),
                 base::Bind(&CanMakePaymentQuery::ExpireQuotaForFrameOrigin,
                            base::Unretained(this), id));
    timers_.insert(std::make_pair(id, std::move(timer)));
    queries_.insert(std::make_pair(id, query));
    return true;
  }

  return it->second == query;
}

void CanMakePaymentQuery::ExpireQuotaForFrameOrigin(const std::string& id) {
  timers_.erase(id);
  queries_.erase(id);
}

}  // namespace payments
