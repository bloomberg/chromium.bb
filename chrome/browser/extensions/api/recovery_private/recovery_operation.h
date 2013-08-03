// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_RECOVERY_OPERATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_RECOVERY_OPERATION_H_

#include "base/callback.h"
#include "base/timer/timer.h"
#include "chrome/common/extensions/api/recovery_private.h"

namespace recovery_api = extensions::api::recovery_private;

namespace extensions {
namespace recovery {

class RecoveryOperationManager;

// Encapsulates an operation being run on behalf of the
// RecoveryOperationManager.  Construction of the operation does not start
// anything.  The operation's Start method should be called to start it, and
// then the Cancel method will stop it.  The operation will call back to the
// RecoveryOperationManager periodically or on any significant event.
class RecoveryOperation {
 public:
  typedef base::Callback<void(bool)> WriteStartCallback;
  typedef base::Callback<void(bool)> WriteCancelCallback;
  typedef std::string ExtensionId;

  RecoveryOperation(RecoveryOperationManager* manager,
                    const ExtensionId& extension_id);
  virtual ~RecoveryOperation();

  // Starts the operation.
  virtual void Start(const WriteStartCallback& callback);
  // Stops the operation gracefully, as if by a user action.
  virtual void Cancel(const WriteCancelCallback& callback);
  // Aborts the operation, generating an error.
  virtual void Abort();
 protected:

  RecoveryOperationManager* manager_;
  const ExtensionId extension_id_;

  recovery_api::Stage stage_;
  int progress_;

  // Timer for stubbing.
  base::RepeatingTimer<RecoveryOperation> timer_;

  // Timer callback for stubbing.
  void UpdateProgress();
};

} // namespace recovery
} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_RECOVERY_OPERATION_H_
