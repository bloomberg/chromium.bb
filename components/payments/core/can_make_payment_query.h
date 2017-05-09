// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_CAN_MAKE_PAYMENT_QUERY_H_
#define COMPONENTS_PAYMENTS_CORE_CAN_MAKE_PAYMENT_QUERY_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace payments {

// Keeps track of canMakePayment() queries per browser context.
class CanMakePaymentQuery : public KeyedService {
 public:
  CanMakePaymentQuery();
  ~CanMakePaymentQuery() override;

  // Returns whether |frame_origin| can call canMakePayment() with |query|,
  // which is a mapping of payment method names to the corresponding
  // JSON-stringified payment method data. Remembers the frame-to-query mapping
  // for 30 minutes to enforce the quota.
  bool CanQuery(const GURL& frame_origin,
                const std::map<std::string, std::set<std::string>>& query);

 private:
  void ExpireQuotaForFrameOrigin(const GURL& frame_origin);

  // A mapping of frame origin to the timer that, when fired, allows the frame
  // to invoke canMakePayment() with a different query.
  std::map<GURL, std::unique_ptr<base::OneShotTimer>> timers_;

  // A mapping of frame origin to its last query. Each query is a mapping of
  // payment method names to the corresponding JSON-stringified payment method
  // data.
  std::map<GURL, std::map<std::string, std::set<std::string>>> queries_;

  DISALLOW_COPY_AND_ASSIGN(CanMakePaymentQuery);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_CAN_MAKE_PAYMENT_QUERY_H_
