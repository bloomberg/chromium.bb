// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/settings/timezone_settings.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/task_runner.h"
#include "base/threading/worker_pool.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace {

// The filepath to the timezone file that symlinks to the actual timezone file.
const char kTimezoneSymlink[] = "/var/lib/timezone/localtime";
const char kTimezoneSymlink2[] = "/var/lib/timezone/localtime2";

// The directory that contains all the timezone files. So for timezone
// "US/Pacific", the actual timezone file is: "/usr/share/zoneinfo/US/Pacific"
const char kTimezoneFilesDir[] = "/usr/share/zoneinfo/";

// Fallback time zone ID used in case of an unexpected error.
const char kFallbackTimeZoneId[] = "America/Los_Angeles";

// TODO(jungshik): Using Enumerate method in ICU gives 600+ timezones.
// Even after filtering out duplicate entries with a strict identity check,
// we still have 400+ zones. Relaxing the criteria for the timezone
// identity is likely to cut down the number to < 100. Until we
// come up with a better list, we hard-code the following list. It came from
// from Android initially, but more entries have been added.
static const char* kTimeZones[] = {
    "Pacific/Midway",
    "Pacific/Honolulu",
    "America/Anchorage",
    "America/Los_Angeles",
    "America/Vancouver",
    "America/Tijuana",
    "America/Phoenix",
    "America/Chihuahua",
    "America/Denver",
    "America/Edmonton",
    "America/Mazatlan",
    "America/Regina",
    "America/Costa_Rica",
    "America/Chicago",
    "America/Mexico_City",
    "America/Winnipeg",
    "Pacific/Easter",
    "America/Bogota",
    "America/Lima",
    "America/New_York",
    "America/Toronto",
    "America/Caracas",
    "America/Barbados",
    "America/Halifax",
    "America/Manaus",
    "America/Santiago",
    "America/St_Johns",
    "America/Araguaina",
    "America/Argentina/Buenos_Aires",
    "America/Argentina/San_Luis",
    "America/Sao_Paulo",
    "America/Montevideo",
    "America/Godthab",
    "Atlantic/South_Georgia",
    "Atlantic/Cape_Verde",
    "Atlantic/Azores",
    "Atlantic/Reykjavik",
    "Atlantic/St_Helena",
    "Africa/Casablanca",
    "Atlantic/Faroe",
    "Europe/Dublin",
    "Europe/Lisbon",
    "Europe/London",
    "Europe/Amsterdam",
    "Europe/Belgrade",
    "Europe/Berlin",
    "Europe/Brussels",
    "Europe/Budapest",
    "Europe/Copenhagen",
    "Europe/Ljubljana",
    "Europe/Madrid",
    "Europe/Oslo",
    "Europe/Paris",
    "Europe/Prague",
    "Europe/Rome",
    "Europe/Stockholm",
    "Europe/Sarajevo",
    "Europe/Tirane",
    "Europe/Vienna",
    "Europe/Warsaw",
    "Europe/Zurich",
    "Africa/Windhoek",
    "Africa/Lagos",
    "Africa/Brazzaville",
    "Africa/Cairo",
    "Africa/Harare",
    "Africa/Maputo",
    "Africa/Johannesburg",
    "Europe/Athens",
    "Europe/Bucharest",
    "Europe/Chisinau",
    "Europe/Helsinki",
    "Europe/Istanbul",
    "Europe/Kiev",
    "Europe/Riga",
    "Europe/Sofia",
    "Europe/Tallinn",
    "Europe/Vilnius",
    "Asia/Amman",
    "Asia/Beirut",
    "Asia/Jerusalem",
    "Africa/Nairobi",
    "Asia/Baghdad",
    "Asia/Riyadh",
    "Asia/Kuwait",
    "Europe/Minsk",
    "Asia/Tehran",
    "Europe/Moscow",
    "Asia/Dubai",
    "Asia/Tbilisi",
    "Indian/Mauritius",
    "Asia/Baku",
    "Asia/Yerevan",
    "Asia/Kabul",
    "Asia/Karachi",
    "Asia/Ashgabat",
    "Asia/Oral",
    "Asia/Calcutta",
    "Asia/Colombo",
    "Asia/Katmandu",
    "Asia/Yekaterinburg",
    "Asia/Almaty",
    "Asia/Dhaka",
    "Asia/Rangoon",
    "Asia/Bangkok",
    "Asia/Jakarta",
    "Asia/Omsk",
    "Asia/Novosibirsk",
    "Asia/Ho_Chi_Minh",
    "Asia/Phnom_Penh",
    "Asia/Vientiane",
    "Asia/Shanghai",
    "Asia/Hong_Kong",
    "Asia/Kuala_Lumpur",
    "Asia/Singapore",
    "Asia/Manila",
    "Asia/Taipei",
    "Asia/Makassar",
    "Asia/Krasnoyarsk",
    "Australia/Perth",
    "Australia/Eucla",
    "Asia/Irkutsk",
    "Asia/Seoul",
    "Asia/Tokyo",
    "Asia/Jayapura",
    "Australia/Darwin",
    "Australia/Adelaide",
    "Asia/Yakutsk",
    "Pacific/Guam",
    "Australia/Brisbane",
    "Australia/Hobart",
    "Australia/Sydney",
    "Pacific/Port_Moresby",
    "Asia/Vladivostok",
    "Asia/Sakhalin",
    "Asia/Magadan",
    "Pacific/Fiji",
    "Pacific/Majuro",
    "Pacific/Auckland",
    "Pacific/Tongatapu",
    "Pacific/Apia",
    "Pacific/Kiritimati",
};

std::string GetTimezoneIDAsString() {
  // Compare with chromiumos/src/platform/init/ui.conf which fixes certain
  // incorrect states of the timezone symlink on startup. Thus errors occuring
  // here should be rather contrived.

  // Look at kTimezoneSymlink, see which timezone we are symlinked to.
  char buf[256];
  const ssize_t len = readlink(kTimezoneSymlink, buf,
                               sizeof(buf)-1);
  if (len == -1) {
    LOG(ERROR) << "GetTimezoneID: Cannot read timezone symlink "
               << kTimezoneSymlink;
    return std::string();
  }

  std::string timezone(buf, len);
  // Remove kTimezoneFilesDir from the beginning.
  if (timezone.find(kTimezoneFilesDir) != 0) {
    LOG(ERROR) << "GetTimezoneID: Timezone symlink is wrong "
               << timezone;
    return std::string();
  }

  return timezone.substr(strlen(kTimezoneFilesDir));
}

void SetTimezoneIDFromString(const std::string& id) {
  // Change the kTimezoneSymlink symlink to the path for this timezone.
  // We want to do this in an atomic way. So we are going to create the symlink
  // at kTimezoneSymlink2 and then move it to kTimezoneSymlink

  base::FilePath timezone_symlink(kTimezoneSymlink);
  base::FilePath timezone_symlink2(kTimezoneSymlink2);
  base::FilePath timezone_file(kTimezoneFilesDir + id);

  // Make sure timezone_file exists.
  if (!base::PathExists(timezone_file)) {
    LOG(ERROR) << "SetTimezoneID: Cannot find timezone file "
               << timezone_file.value();
    return;
  }

  // Delete old symlink2 if it exists.
  base::DeleteFile(timezone_symlink2, false);

  // Create new symlink2.
  if (symlink(timezone_file.value().c_str(),
              timezone_symlink2.value().c_str()) == -1) {
    LOG(ERROR) << "SetTimezoneID: Unable to create symlink "
               << timezone_symlink2.value() << " to " << timezone_file.value();
    return;
  }

  // Move symlink2 to symlink.
  if (!base::ReplaceFile(timezone_symlink2, timezone_symlink, NULL)) {
    LOG(ERROR) << "SetTimezoneID: Unable to move symlink "
               << timezone_symlink2.value() << " to "
               << timezone_symlink.value();
  }
}

// Common code of the TimezoneSettings implementations.
class TimezoneSettingsBaseImpl : public chromeos::system::TimezoneSettings {
 public:
  virtual ~TimezoneSettingsBaseImpl();

  // TimezoneSettings implementation:
  virtual const icu::TimeZone& GetTimezone() OVERRIDE;
  virtual base::string16 GetCurrentTimezoneID() OVERRIDE;
  virtual void SetTimezoneFromID(const base::string16& timezone_id) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual const std::vector<icu::TimeZone*>& GetTimezoneList() const OVERRIDE;

 protected:
  TimezoneSettingsBaseImpl();

  // Returns |timezone| if it is an element of |timezones_|.
  // Otherwise, returns a timezone from |timezones_|, if such exists, that has
  // the same rule as the given |timezone|.
  // Otherwise, returns NULL.
  // Note multiple timezones with the same time zone offset may exist
  // e.g.
  //   US/Pacific == America/Los_Angeles
  const icu::TimeZone* GetKnownTimezoneOrNull(
      const icu::TimeZone& timezone) const;

  ObserverList<Observer> observers_;
  std::vector<icu::TimeZone*> timezones_;
  scoped_ptr<icu::TimeZone> timezone_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimezoneSettingsBaseImpl);
};

// The TimezoneSettings implementation used in production.
class TimezoneSettingsImpl : public TimezoneSettingsBaseImpl {
 public:
  // TimezoneSettings implementation:
  virtual void SetTimezone(const icu::TimeZone& timezone) OVERRIDE;

  static TimezoneSettingsImpl* GetInstance();

 private:
  friend struct DefaultSingletonTraits<TimezoneSettingsImpl>;

  TimezoneSettingsImpl();

  DISALLOW_COPY_AND_ASSIGN(TimezoneSettingsImpl);
};

// The stub TimezoneSettings implementation used on Linux desktop.
class TimezoneSettingsStubImpl : public TimezoneSettingsBaseImpl {
 public:
  // TimezoneSettings implementation:
  virtual void SetTimezone(const icu::TimeZone& timezone) OVERRIDE;

  static TimezoneSettingsStubImpl* GetInstance();

 private:
  friend struct DefaultSingletonTraits<TimezoneSettingsStubImpl>;

  TimezoneSettingsStubImpl();

  DISALLOW_COPY_AND_ASSIGN(TimezoneSettingsStubImpl);
};

TimezoneSettingsBaseImpl::~TimezoneSettingsBaseImpl() {
  STLDeleteElements(&timezones_);
}

const icu::TimeZone& TimezoneSettingsBaseImpl::GetTimezone() {
  return *timezone_.get();
}

base::string16 TimezoneSettingsBaseImpl::GetCurrentTimezoneID() {
  return chromeos::system::TimezoneSettings::GetTimezoneID(GetTimezone());
}

void TimezoneSettingsBaseImpl::SetTimezoneFromID(
    const base::string16& timezone_id) {
  scoped_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone(
      icu::UnicodeString(timezone_id.c_str(), timezone_id.size())));
  SetTimezone(*timezone);
}

void TimezoneSettingsBaseImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TimezoneSettingsBaseImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const std::vector<icu::TimeZone*>&
TimezoneSettingsBaseImpl::GetTimezoneList() const {
  return timezones_;
}

TimezoneSettingsBaseImpl::TimezoneSettingsBaseImpl() {
  for (size_t i = 0; i < arraysize(kTimeZones); ++i) {
    timezones_.push_back(icu::TimeZone::createTimeZone(
        icu::UnicodeString(kTimeZones[i], -1, US_INV)));
  }
}

const icu::TimeZone* TimezoneSettingsBaseImpl::GetKnownTimezoneOrNull(
    const icu::TimeZone& timezone) const {
  const icu::TimeZone* known_timezone = NULL;
  for (std::vector<icu::TimeZone*>::const_iterator iter = timezones_.begin();
       iter != timezones_.end(); ++iter) {
    const icu::TimeZone* entry = *iter;
    if (*entry == timezone)
      return entry;
    if (entry->hasSameRules(timezone))
      known_timezone = entry;
  }

  // May return NULL if we did not find a matching timezone in our list.
  return known_timezone;
}

void TimezoneSettingsImpl::SetTimezone(const icu::TimeZone& timezone) {
  // Replace |timezone| by a known timezone with the same rules. If none exists
  // go on with |timezone|.
  const icu::TimeZone* known_timezone = GetKnownTimezoneOrNull(timezone);
  if (!known_timezone)
    known_timezone = &timezone;

  timezone_.reset(known_timezone->clone());
  std::string id = base::UTF16ToUTF8(GetTimezoneID(*known_timezone));
  VLOG(1) << "Setting timezone to " << id;
  // It's safe to change the timezone config files in the background as the
  // following operations don't depend on the completion of the config change.
  base::WorkerPool::GetTaskRunner(true /* task is slow */)->
      PostTask(FROM_HERE, base::Bind(&SetTimezoneIDFromString, id));
  icu::TimeZone::setDefault(*known_timezone);
  FOR_EACH_OBSERVER(Observer, observers_, TimezoneChanged(*known_timezone));
}

// static
TimezoneSettingsImpl* TimezoneSettingsImpl::GetInstance() {
  return Singleton<TimezoneSettingsImpl,
                   DefaultSingletonTraits<TimezoneSettingsImpl> >::get();
}

TimezoneSettingsImpl::TimezoneSettingsImpl() {
  std::string id = GetTimezoneIDAsString();
  if (id.empty()) {
    id = kFallbackTimeZoneId;
    LOG(ERROR) << "Got an empty string for timezone, default to '" << id;
  }

  timezone_.reset(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8(id)));

  // Store a known timezone equivalent to id in |timezone_|.
  const icu::TimeZone* known_timezone = GetKnownTimezoneOrNull(*timezone_);
  if (known_timezone != NULL && *known_timezone != *timezone_)
    // Not necessary to update the filesystem because |known_timezone| has the
    // same rules.
    timezone_.reset(known_timezone->clone());

  icu::TimeZone::setDefault(*timezone_);
  VLOG(1) << "Timezone initially set to " << id;
}

void TimezoneSettingsStubImpl::SetTimezone(const icu::TimeZone& timezone) {
  // Replace |timezone| by a known timezone with the same rules. If none exists
  // go on with |timezone|.
  const icu::TimeZone* known_timezone = GetKnownTimezoneOrNull(timezone);
  if (!known_timezone)
    known_timezone = &timezone;

  timezone_.reset(known_timezone->clone());
  icu::TimeZone::setDefault(*known_timezone);
  FOR_EACH_OBSERVER(Observer, observers_, TimezoneChanged(*known_timezone));
}

// static
TimezoneSettingsStubImpl* TimezoneSettingsStubImpl::GetInstance() {
  return Singleton<TimezoneSettingsStubImpl,
      DefaultSingletonTraits<TimezoneSettingsStubImpl> >::get();
}

TimezoneSettingsStubImpl::TimezoneSettingsStubImpl() {
  timezone_.reset(icu::TimeZone::createDefault());
  const icu::TimeZone* known_timezone = GetKnownTimezoneOrNull(*timezone_);
  if (known_timezone != NULL && *known_timezone != *timezone_)
    timezone_.reset(known_timezone->clone());
}

}  // namespace

namespace chromeos {
namespace system {

TimezoneSettings::Observer::~Observer() {}

// static
TimezoneSettings* TimezoneSettings::GetInstance() {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    return TimezoneSettingsImpl::GetInstance();
  } else {
    return TimezoneSettingsStubImpl::GetInstance();
  }
}

// static
base::string16 TimezoneSettings::GetTimezoneID(const icu::TimeZone& timezone) {
  icu::UnicodeString id;
  timezone.getID(id);
  return base::string16(id.getBuffer(), id.length());
}

}  // namespace system
}  // namespace chromeos
