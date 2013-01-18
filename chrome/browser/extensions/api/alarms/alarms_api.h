// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class AlarmsCreateFunction : public SyncExtensionFunction {
  typedef base::Time (*TimeProvider)();
 public:
  AlarmsCreateFunction();
  explicit AlarmsCreateFunction(TimeProvider now) : now_(now) {}
 protected:
  virtual ~AlarmsCreateFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION("alarms.create", ALARMS_CREATE)
 private:
  TimeProvider now_;
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
