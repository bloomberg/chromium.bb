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

namespace payments {

CanMakePaymentQuery::CanMakePaymentQuery() {}

CanMakePaymentQuery::~CanMakePaymentQuery() {}

bool CanMakePaymentQuery::CanQuery(
    const GURL& frame_origin,
    const std::map<std::string, std::set<std::string>>& query) {
  const auto& it = queries_.find(frame_origin);
  if (it == queries_.end()) {
    std::unique_ptr<base::OneShotTimer> timer =
        base::MakeUnique<base::OneShotTimer>();
    timer->Start(FROM_HERE, base::TimeDelta::FromMinutes(30),
                 base::Bind(&CanMakePaymentQuery::ExpireQuotaForFrameOrigin,
                            base::Unretained(this), frame_origin));
    timers_.insert(std::make_pair(frame_origin, std::move(timer)));
    queries_.insert(std::make_pair(frame_origin, query));
    return true;
  }

  return it->second == query;
}

void CanMakePaymentQuery::ExpireQuotaForFrameOrigin(const GURL& frame_origin) {
  timers_.erase(frame_origin);
  queries_.erase(frame_origin);
}

}  // namespace payments
