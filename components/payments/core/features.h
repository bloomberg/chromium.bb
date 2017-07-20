// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_FEATURES_H_
#define COMPONENTS_PAYMENTS_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace payments {
namespace features {

#if defined(OS_IOS)
// Used to control the state of the Payment Request API feature.
extern const base::Feature kWebPayments;
#endif

// Used to control the support for Payment Details modifiers.
extern const base::Feature kWebPaymentsModifiers;

}  // namespace features
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_FEATURES_H_
