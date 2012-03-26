// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RLZ_RLZ_EXTENSION_API_H_
#define CHROME_BROWSER_RLZ_RLZ_EXTENSION_API_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN) || defined(OS_MACOSX)

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function.h"
#include "rlz/lib/lib_values.h"

class RlzRecordProductEventFunction : public SyncExtensionFunction {
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.rlz.recordProductEvent")
};

class RlzGetAccessPointRlzFunction : public SyncExtensionFunction {
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.rlz.getAccessPointRlz")
};

class RlzSendFinancialPingFunction : public AsyncExtensionFunction {
 public:
  RlzSendFinancialPingFunction();
  virtual ~RlzSendFinancialPingFunction();

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.rlz.sendFinancialPing")
 // Making this function protected so that it can be overridden in tests.
 protected:
  virtual bool RunImpl() OVERRIDE;

 private:
  void WorkOnWorkerThread();
  void RespondOnUIThread();

  rlz_lib::Product product_;
  scoped_array<rlz_lib::AccessPoint> access_points_;
  std::string signature_;
  std::string brand_;
  std::string id_;
  std::string lang_;
  bool exclude_machine_id_;
};

class RlzClearProductStateFunction : public SyncExtensionFunction {
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.rlz.clearProductState")
};

#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#endif  // CHROME_BROWSER_RLZ_RLZ_EXTENSION_API_H_
