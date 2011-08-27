// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of SyncNotifier that wraps InvalidationNotifier
// on its own thread.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_NON_BLOCKING_INVALIDATION_NOTIFIER_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_NON_BLOCKING_INVALIDATION_NOTIFIER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "jingle/notifier/base/notifier_options.h"

namespace base {
class MessageLoopProxy;
}

namespace sync_notifier {

class NonBlockingInvalidationNotifier : public SyncNotifier {
 public:
  NonBlockingInvalidationNotifier(
      const notifier::NotifierOptions& notifier_options,
      const std::string& client_info);

  virtual ~NonBlockingInvalidationNotifier();

  // SyncNotifier implementation.
  virtual void AddObserver(SyncNotifierObserver* observer) OVERRIDE;
  virtual void RemoveObserver(SyncNotifierObserver* observer) OVERRIDE;
  virtual void SetUniqueId(const std::string& unique_id) OVERRIDE;
  virtual void SetState(const std::string& state) OVERRIDE;
  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE;
  virtual void UpdateEnabledTypes(
      const syncable::ModelTypeSet& enabled_types) OVERRIDE;
  virtual void SendNotification(
      const syncable::ModelTypeSet& changed_types) OVERRIDE;

 private:
  // The real guts of NonBlockingInvalidationNotifier, which allows this class
  // to not be refcounted.
  class Core;
  scoped_refptr<Core> core_;
  scoped_refptr<base::MessageLoopProxy> parent_message_loop_proxy_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  DISALLOW_COPY_AND_ASSIGN(NonBlockingInvalidationNotifier);
};

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_NON_BLOCKING_INVALIDATION_NOTIFIER_H_
