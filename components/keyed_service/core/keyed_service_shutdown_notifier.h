// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_SHUTDOWN_NOTIFIER_H_
#define COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_SHUTDOWN_NOTIFIER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

// This is a helper class for objects that depend on one or more keyed services,
// but which cannot be keyed services themselves, for example because they don't
// correspond 1:1 to a context, or because they have a different lifetime.
//
// To use this class, add a factory class and declare the dependencies there.
// This class (being a KeyedService itself) will be shut down before its
// dependencies and notify its observers.
class KEYED_SERVICE_EXPORT KeyedServiceShutdownNotifier : public KeyedService {
 public:
  using Subscription = base::CallbackList<void()>::Subscription;

  KeyedServiceShutdownNotifier();
  ~KeyedServiceShutdownNotifier() override;

  // Subscribe for a notification when the keyed services this object depends on
  // (as defined by its factory) are shut down. The subscription object can be
  // destroyed to unsubscribe.
  std::unique_ptr<Subscription> Subscribe(
      const base::RepeatingClosure& callback);

 private:
  // KeyedService implementation:
  void Shutdown() override;

  base::CallbackList<void()> callback_list_;

  DISALLOW_COPY_AND_ASSIGN(KeyedServiceShutdownNotifier);
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_SHUTDOWN_NOTIFIER_H_
