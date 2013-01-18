// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RLZ_RLZ_EXTENSION_API_H_
#define CHROME_BROWSER_RLZ_RLZ_EXTENSION_API_H_

#include "build/build_config.h"

#if defined(ENABLE_RLZ)

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function.h"
#include "rlz/lib/lib_values.h"

class RlzRecordProductEventFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.rlz.recordProductEvent",
                             EXPERIMENTAL_RLZ_RECORDPRODUCTEVENT)

 protected:
  virtual ~RlzRecordProductEventFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class RlzGetAccessPointRlzFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.rlz.getAccessPointRlz",
                             EXPERIMENTAL_RLZ_GETACCESSPOINTRLZ)

 protected:
  virtual ~RlzGetAccessPointRlzFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class RlzSendFinancialPingFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.rlz.sendFinancialPing",
                             EXPERIMENTAL_RLZ_SENDFINANCIALPING)

  RlzSendFinancialPingFunction();

 protected:
  friend class MockRlzSendFinancialPingFunction;
  virtual ~RlzSendFinancialPingFunction();

  // ExtensionFunction:
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
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.rlz.clearProductState",
                             EXPERIMENTAL_RLZ_CLEARPRODUCTSTATE)

 protected:
  virtual ~RlzClearProductStateFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

#endif  // defined(ENABLE_RLZ)

#endif  // CHROME_BROWSER_RLZ_RLZ_EXTENSION_API_H_
