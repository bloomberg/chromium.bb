// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_POLICY_HELPERS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_POLICY_HELPERS_H_

#include <map>
#include <string>

#include "base/optional.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace base {
class TimeDelta;
class Value;
}  // namespace base

namespace chromeos {
namespace app_time {

class AppId;
class AppLimit;
enum class AppRestriction;

namespace policy {

// Dictionary keys for app time limits related policies.
extern const char kUrlList[];
extern const char kAppList[];
extern const char kAppId[];
extern const char kAppType[];
extern const char kAppLimitsArray[];
extern const char kAppInfoDict[];
extern const char kRestrictionEnum[];
extern const char kDailyLimitInt[];
extern const char kLastUpdatedString[];
extern const char kResetAtDict[];
extern const char kHourInt[];
extern const char kMinInt[];
extern const char kActivityReportingEnabled[];

// Converts between apps::mojom::AppType and string used by app time limits
// policies.
apps::mojom::AppType PolicyStringToAppType(const std::string& app_type);
std::string AppTypeToPolicyString(apps::mojom::AppType app_type);

// Converts between AppRestriction and string used by app time limits policies.
AppRestriction PolicyStringToAppRestriction(const std::string& restriction);
std::string AppRestrictionToPolicyString(const AppRestriction& restriction);

// Deserializes AppId from |dict|.
// Returns value if |dict| contains valid app information.
base::Optional<AppId> AppIdFromDict(const base::Value& dict);

// Serializes AppId to the dictionary.
base::Value AppIdToDict(const AppId& app_id);

// Deserializes AppId from |dict|.
// Returns value if |dict| contains valid app information in its entry keyed by
// kAppInfoDict.
base::Optional<AppId> AppIdFromAppInfoDict(const base::Value& dict);

// Deserializes AppLimit from |dict|.
// Returns value if |dict| contains valid app limit information.
base::Optional<AppLimit> AppLimitFromDict(const base::Value& dict);

// Serializes AppLimit to the dictionary.
base::Value AppLimitToDict(const AppLimit& limit);

// Deserializes daily limits reset time from |dict|.
// Returns value if |dict| contains valid reset time information.
base::Optional<base::TimeDelta> ResetTimeFromDict(const base::Value& dict);

// Serializes daily limits reset to the dictionary.
base::Value ResetTimeToDict(int hour, int minutes);

// Deserializes activity reporting enabled boolean from |dict|.
// Returns value if |dict| contains a valid entry.
base::Optional<bool> ActivityReportingEnabledFromDict(const base::Value& dict);

// Deserializes app limits data from the |dict|.
// Will return empty map if |dict| is invalid.
std::map<AppId, AppLimit> AppLimitsFromDict(const base::Value& dict);

}  // namespace policy
}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_POLICY_HELPERS_H_
