// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/timezone_settings.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_thread.h"

namespace chromeos {
namespace system {

namespace {

// The filepath to the timezone file that symlinks to the actual timezone file.
const char kTimezoneSymlink[] = "/var/lib/timezone/localtime";
const char kTimezoneSymlink2[] = "/var/lib/timezone/localtime2";

// The directory that contains all the timezone files. So for timezone
// "US/Pacific", the actual timezone file is: "/usr/share/zoneinfo/US/Pacific"
const char kTimezoneFilesDir[] = "/usr/share/zoneinfo/";

// Fallback time zone ID used in case of an unexpected error.
const char kFallbackTimeZoneId[] = "America/Los_Angeles";

}  // namespace

class TimezoneSettingsImpl : public TimezoneSettings {
 public:
  // TimezoneSettings.implementation:
  virtual const icu::TimeZone& GetTimezone();
  virtual void SetTimezone(const icu::TimeZone& timezone);
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  static TimezoneSettingsImpl* GetInstance();

 private:
  friend struct DefaultSingletonTraits<TimezoneSettingsImpl>;

  TimezoneSettingsImpl();

  scoped_ptr<icu::TimeZone> timezone_;
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(TimezoneSettingsImpl);
};

std::string GetTimezoneIDAsString() {
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

  FilePath timezone_symlink(kTimezoneSymlink);
  FilePath timezone_symlink2(kTimezoneSymlink2);
  FilePath timezone_file(kTimezoneFilesDir + id);

  // Make sure timezone_file exists.
  if (!file_util::PathExists(timezone_file)) {
    LOG(ERROR) << "SetTimezoneID: Cannot find timezone file "
               << timezone_file.value();
    return;
  }

  // Delete old symlink2 if it exists.
  file_util::Delete(timezone_symlink2, false);

  // Create new symlink2.
  if (symlink(timezone_file.value().c_str(),
              timezone_symlink2.value().c_str()) == -1) {
    LOG(ERROR) << "SetTimezoneID: Unable to create symlink "
               << timezone_symlink2.value() << " to " << timezone_file.value();
    return;
  }

  // Move symlink2 to symlink.
  if (!file_util::ReplaceFile(timezone_symlink2, timezone_symlink)) {
    LOG(ERROR) << "SetTimezoneID: Unable to move symlink "
               << timezone_symlink2.value() << " to "
               << timezone_symlink.value();
  }
}

const icu::TimeZone& TimezoneSettingsImpl::GetTimezone() {
  return *timezone_.get();
}

void TimezoneSettingsImpl::SetTimezone(const icu::TimeZone& timezone) {
  timezone_.reset(timezone.clone());
  icu::UnicodeString unicode;
  timezone.getID(unicode);
  std::string id;
  UTF16ToUTF8(unicode.getBuffer(), unicode.length(), &id);
  VLOG(1) << "Setting timezone to " << id;
  // Change the timezone config files on the FILE thread. It's safe to do this
  // in the background as the following operations don't depend on the
  // completion of the config change.
  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&SetTimezoneIDFromString, id));
  icu::TimeZone::setDefault(timezone);
  FOR_EACH_OBSERVER(Observer, observers_, TimezoneChanged(timezone));
}

void TimezoneSettingsImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TimezoneSettingsImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

TimezoneSettingsImpl::TimezoneSettingsImpl() {
  // Get Timezone
  std::string id = GetTimezoneIDAsString();
  if (id.empty()) {
    id = kFallbackTimeZoneId;
    LOG(ERROR) << "Got an empty string for timezone, default to " << id;
  }
  icu::TimeZone* timezone =
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(id));
  timezone_.reset(timezone);
  icu::TimeZone::setDefault(*timezone);
  VLOG(1) << "Timezone is " << id;
}

TimezoneSettingsImpl* TimezoneSettingsImpl::GetInstance() {
  return Singleton<TimezoneSettingsImpl,
                   DefaultSingletonTraits<TimezoneSettingsImpl> >::get();
}

TimezoneSettings* TimezoneSettings::GetInstance() {
  return TimezoneSettingsImpl::GetInstance();
}

}  // namespace system
}  // namespace chromeos

// Allows InvokeLater without adding refcounting. TimezoneSettingsImpl is a
// Singleton and won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::system::TimezoneSettingsImpl);
