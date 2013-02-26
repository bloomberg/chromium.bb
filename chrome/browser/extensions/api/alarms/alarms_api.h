// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__

#include "chrome/browser/extensions/extension_function.h"

namespace base {
class Clock;
}  // namespace base

namespace extensions {

class AlarmsCreateFunction : public SyncExtensionFunction {
 public:
  AlarmsCreateFunction();
  // Use |clock| instead of the default clock. Does not take ownership
  // of |clock|. Used for testing.
  explicit AlarmsCreateFunction(base::Clock* clock);
 protected:
  virtual ~AlarmsCreateFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("alarms.create", ALARMS_CREATE)
 private:
  base::Clock* const clock_;
  // Whether or not we own |clock_|. This is needed because we own it
  // when we create it ourselves, but not when it's passed in for
  // testing.
  bool owns_clock_;
};

class AlarmsGetFunction : public SyncExtensionFunction {
 protected:
  virtual ~AlarmsGetFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("alarms.get", ALARMS_GET)
};

class AlarmsGetAllFunction : public SyncExtensionFunction {
 protected:
  virtual ~AlarmsGetAllFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("alarms.getAll", ALARMS_GETALL)
};

class AlarmsClearFunction : public SyncExtensionFunction {
 protected:
  virtual ~AlarmsClearFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("alarms.clear", ALARMS_CLEAR)
};

class AlarmsClearAllFunction : public SyncExtensionFunction {
 protected:
  virtual ~AlarmsClearAllFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("alarms.clearAll", ALARMS_CLEARALL)
};

} //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__
