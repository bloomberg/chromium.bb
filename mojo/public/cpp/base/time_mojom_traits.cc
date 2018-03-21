// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

int64_t StructTraits<mojo_base::mojom::TimeDataView,
                     base::Time>::internal_value(const base::Time& time) {
  return time.since_origin().InMicroseconds();
}

bool StructTraits<mojo_base::mojom::TimeDataView, base::Time>::Read(
    mojo_base::mojom::TimeDataView data,
    base::Time* time) {
  *time =
      base::Time() + base::TimeDelta::FromMicroseconds(data.internal_value());
  return true;
}

int64_t
StructTraits<mojo_base::mojom::TimeDeltaDataView,
             base::TimeDelta>::microseconds(const base::TimeDelta& delta) {
  return delta.InMicroseconds();
}

bool StructTraits<mojo_base::mojom::TimeDeltaDataView, base::TimeDelta>::Read(
    mojo_base::mojom::TimeDeltaDataView data,
    base::TimeDelta* delta) {
  *delta = base::TimeDelta::FromMicroseconds(data.microseconds());
  return true;
}

int64_t
StructTraits<mojo_base::mojom::TimeTicksDataView,
             base::TimeTicks>::internal_value(const base::TimeTicks& time) {
  return time.since_origin().InMicroseconds();
}

bool StructTraits<mojo_base::mojom::TimeTicksDataView, base::TimeTicks>::Read(
    mojo_base::mojom::TimeTicksDataView data,
    base::TimeTicks* time) {
  *time = base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(data.internal_value());
  return true;
}

}  // namespace mojo