// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SYSTEM_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SYSTEM_LIBRARY_H_

#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "cros/chromeos_system.h"
#include "unicode/timezone.h"

namespace chromeos {

// This interface defines interaction with the ChromeOS system APIs.
class SystemLibrary {
 public:
  class Observer {
   public:
    // Called when the timezone has changed. |timezone| is non-null.
    virtual void TimezoneChanged(const icu::TimeZone& timezone) = 0;
  };

  virtual ~SystemLibrary() {}

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the current timezone as an icu::Timezone object.
  virtual const icu::TimeZone& GetTimezone() = 0;

  // Sets the current timezone. |timezone| must be non-null.
  virtual void SetTimezone(const icu::TimeZone* timezone) = 0;
};

// This class handles the interaction with the ChromeOS syslogs APIs.
class SystemLibraryImpl : public SystemLibrary {
 public:
  SystemLibraryImpl();
  virtual ~SystemLibraryImpl() {}

  // NetworkLibrary overrides.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  virtual const icu::TimeZone& GetTimezone();
  virtual void SetTimezone(const icu::TimeZone*);

 private:
  scoped_ptr<icu::TimeZone> timezone_;
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(SystemLibraryImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SYSTEM_LIBRARY_H_
