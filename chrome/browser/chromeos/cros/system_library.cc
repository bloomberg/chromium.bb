// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/system_library.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

namespace chromeos {

class SystemLibraryImpl : public SystemLibrary {
 public:
  SystemLibraryImpl() {
    std::string id = "US/Pacific";
    if (CrosLibrary::Get()->EnsureLoaded()) {
      std::string timezone_id = chromeos::GetTimezoneID();
      if (timezone_id.empty()) {
        LOG(ERROR) << "Got an empty string for timezone, default to " << id;
      } else {
        id = timezone_id;
      }
    }
    icu::TimeZone* timezone =
        icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(id));
    timezone_.reset(timezone);
    icu::TimeZone::setDefault(*timezone);
    LOG(INFO) << "Timezone is " << id;
  }

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  const icu::TimeZone& GetTimezone() {
    return *timezone_.get();
  }

  void SetTimezone(const icu::TimeZone* timezone) {
    timezone_.reset(timezone->clone());
    if (CrosLibrary::Get()->EnsureLoaded()) {
      icu::UnicodeString unicode;
      timezone->getID(unicode);
      std::string id;
      UTF16ToUTF8(unicode.getBuffer(), unicode.length(), &id);
      LOG(INFO) << "Setting timezone to " << id;
      chromeos::SetTimezoneID(id);
    }
    icu::TimeZone::setDefault(*timezone);
    FOR_EACH_OBSERVER(Observer, observers_, TimezoneChanged(*timezone));
  }

  private:
   scoped_ptr<icu::TimeZone> timezone_;
   ObserverList<Observer> observers_;

   DISALLOW_COPY_AND_ASSIGN(SystemLibraryImpl);
};

class SystemLibraryStubImpl : public SystemLibrary {
 public:
  SystemLibraryStubImpl() {
    std::string id = "US/Pacific";
    icu::TimeZone* timezone =
        icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(id));
    timezone_.reset(timezone);
  }
  ~SystemLibraryStubImpl() {}

  void AddObserver(Observer* observer) {}
  void RemoveObserver(Observer* observer) {}
  const icu::TimeZone& GetTimezone() {
    return *timezone_.get();
  }
  void SetTimezone(const icu::TimeZone* timezone) {}

  private:
   scoped_ptr<icu::TimeZone> timezone_;
   DISALLOW_COPY_AND_ASSIGN(SystemLibraryStubImpl);
};

// static
SystemLibrary* SystemLibrary::GetImpl(bool stub) {
  if (stub)
    return new SystemLibraryStubImpl();
  else
    return new SystemLibraryImpl();
}

}  // namespace chromeos
