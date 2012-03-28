// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class AlarmsCreateFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.alarms.create");
};

class AlarmsGetFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.alarms.get");
};

class AlarmsGetAllFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.alarms.getAll");
};

class AlarmsClearFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.alarms.clear");
};

class AlarmsClearAllFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.alarms.clearAll");
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARMS_API_H__
