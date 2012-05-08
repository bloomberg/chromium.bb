// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class AlarmsCreateFunction : public SyncExtensionFunction {
 protected:
  virtual ~AlarmsCreateFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("alarms.create");
};

class AlarmsGetFunction : public SyncExtensionFunction {
 protected:
  virtual ~AlarmsGetFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("alarms.get");
};

class AlarmsGetAllFunction : public SyncExtensionFunction {
 protected:
  virtual ~AlarmsGetAllFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("alarms.getAll");
};

class AlarmsClearFunction : public SyncExtensionFunction {
 protected:
  virtual ~AlarmsClearFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("alarms.clear");
};

class AlarmsClearAllFunction : public SyncExtensionFunction {
 protected:
  virtual ~AlarmsClearAllFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("alarms.clearAll");
};

} //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__
