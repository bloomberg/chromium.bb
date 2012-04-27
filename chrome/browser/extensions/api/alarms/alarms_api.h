// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class AlarmsCreateFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.alarms.create");

 protected:
  virtual ~AlarmsCreateFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class AlarmsGetFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.alarms.get");

 protected:
  virtual ~AlarmsGetFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class AlarmsGetAllFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.alarms.getAll");

 protected:
  virtual ~AlarmsGetAllFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class AlarmsClearFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.alarms.clear");

 protected:
  virtual ~AlarmsClearFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class AlarmsClearAllFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.alarms.clearAll");

 protected:
  virtual ~AlarmsClearAllFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

} //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__
