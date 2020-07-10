// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_AUTO_UPDATE_TIME_RESTRICTIONS_DECODER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_AUTO_UPDATE_TIME_RESTRICTIONS_DECODER_H_

#include <memory>
#include <vector>

#include "base/optional.h"
#include "base/values.h"

namespace policy {

class WeeklyTime;
class WeeklyTimeInterval;

// Expects a dictionary value that follows the DisallowedTime schema and then
// converts it into a WeeklyTime object. To see an example go to DisallowedTime
// under DeviceAutoUpdateTimeRestrictions in policy_templates.json.
base::Optional<WeeklyTime> WeeklyTimeFromDictValue(
    const base::DictionaryValue& weekly_time_dict);

// The list value is expected to follow the DeviceAutoUpdateTimeRestrictions
// schema (see policy_templates.json), thus the input should be the result of
// parsing and validating a JSON string. This will then convert the members of
// the DeviceAutoUpdateTimeRestrictions into WeeklyTimeInterval objects. False
// is returned if a type mismatch with the schema is found in any of the members
// from |intervals_list| or if there was an issue retrieving keys from the
// any of them. If there are no errors, the intervals in |intervals_list| will
// be put in |intervals_out|, if false is returned then |intervals_out| may
// contain some valid intervals from |intervals_list|, but it may not be all of
// them.
bool WeeklyTimeIntervalsFromListValue(
    const base::ListValue& intervals_list,
    std::vector<WeeklyTimeInterval>* intervals_out);

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_AUTO_UPDATE_TIME_RESTRICTIONS_DECODER_H_
