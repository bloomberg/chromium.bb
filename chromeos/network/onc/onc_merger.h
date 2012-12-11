// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_MERGER_H_
#define CHROMEOS_NETWORK_ONC_ONC_MERGER_H_

#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace onc {

// Merges the given |user_onc| and |shared_onc| settings with the given
// |user_policy| and |device_policy| settings. Each can be omitted by providing
// a NULL pointer. Each dictionary has to be a valid ONC dictionary. They don't
// have to describe top-level ONC but should refer to the same section in
// ONC. |user_onc| and |shared_onc| should not contain kRecommended fields. The
// resulting dictionary is valid ONC but may contain dispensable fields (e.g. in
// a network with type: "WiFi", the field "VPN" is dispensable) that can be
// removed by the caller using the ONC normalizer. ONC conformance of the
// arguments is not checked. Use ONC validator for that.
CHROMEOS_EXPORT scoped_ptr<base::DictionaryValue> MergeSettingsWithPolicies(
    const base::DictionaryValue* user_policy,
    const base::DictionaryValue* device_policy,
    const base::DictionaryValue* user_onc,
    const base::DictionaryValue* shared_onc);

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_MERGER_H_
