// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_STORABLE_IMPRESSION_H_
#define CONTENT_BROWSER_CONVERSIONS_STORABLE_IMPRESSION_H_

#include <stdint.h>
#include <string>

#include "base/optional.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// Struct which represents all stored attributes of an impression. All values
// should be sanitized before creating this object.
class CONTENT_EXPORT StorableImpression {
 public:
  // If |impression_id| is not available, 0 should be provided.
  StorableImpression(const std::string& impression_data,
                     const url::Origin& impression_origin,
                     const url::Origin& conversion_origin,
                     const url::Origin& reporting_origin,
                     base::Time impression_time,
                     base::Time expiry_time,
                     const base::Optional<int64_t>& impression_id);
  StorableImpression(const StorableImpression& other);
  StorableImpression& operator=(const StorableImpression& other) = delete;
  ~StorableImpression();

  const std::string& impression_data() const { return impression_data_; }

  const url::Origin& impression_origin() const { return impression_origin_; }

  const url::Origin& conversion_origin() const { return conversion_origin_; }

  const url::Origin& reporting_origin() const { return reporting_origin_; }

  base::Time impression_time() const { return impression_time_; }

  base::Time expiry_time() const { return expiry_time_; }

  base::Optional<int64_t> impression_id() const { return impression_id_; }

 private:
  // String representing a valid hexadecimal number.
  std::string impression_data_;
  url::Origin impression_origin_;
  url::Origin conversion_origin_;
  url::Origin reporting_origin_;
  base::Time impression_time_;
  base::Time expiry_time_;

  // If null, an ID has not been assigned yet.
  base::Optional<int64_t> impression_id_;

  // When adding new members, the ImpressionsEqual() testing utility in
  // conversion_test_utils.h should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_STORABLE_IMPRESSION_H_
