// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_RECOVERY_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_RECOVERY_PRIVATE_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/recovery_private.h"

namespace extensions {

class RecoveryPrivateWriteFromUrlFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("recoveryPrivate.writeFromUrl",
                             RECOVERYPRIVATE_WRITEFROMURL)
  RecoveryPrivateWriteFromUrlFunction();

 private:
  virtual ~RecoveryPrivateWriteFromUrlFunction();
  virtual bool RunImpl() OVERRIDE;
  void OnWriteStarted(bool success);
};

class RecoveryPrivateWriteFromFileFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("recoveryPrivate.writeFromFile",
                             RECOVERYPRIVATE_WRITEFROMFILE)
  RecoveryPrivateWriteFromFileFunction();

 private:
  virtual ~RecoveryPrivateWriteFromFileFunction();
  virtual bool RunImpl() OVERRIDE;
  void OnWriteStarted(bool success);
};

class RecoveryPrivateCancelWriteFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("recoveryPrivate.cancelWrite",
                             RECOVERYPRIVATE_CANCELWRITE)
  RecoveryPrivateCancelWriteFunction();

 private:
  virtual ~RecoveryPrivateCancelWriteFunction();
  virtual bool RunImpl() OVERRIDE;
  void OnWriteCancelled(bool success);
};

class RecoveryPrivateDestroyPartitionsFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("recoveryPrivate.destroyPartitions",
                             RECOVERYPRIVATE_DESTROYPARTITIONS)
  RecoveryPrivateDestroyPartitionsFunction();

 private:
  virtual ~RecoveryPrivateDestroyPartitionsFunction();
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_RECOVERY_PRIVATE_API_H_
