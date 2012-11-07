// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_SHILL_SERVICE_OBSERVER_H_
#define CHROMEOS_NETWORK_SHILL_SERVICE_OBSERVER_H_

#include <string>

#include "base/callback.h"
#include "chromeos/dbus/shill_property_changed_observer.h"

namespace chromeos {
namespace internal {

// Class to manage Shill service property changed observers. Observers are
// added on construction and removed on destruction. Runs the handler when
// OnPropertyChanged is called.
class ShillServiceObserver : public ShillPropertyChangedObserver {
 public:
  typedef base::Callback<void(const std::string& service,
                              const std::string& name,
                              const base::Value& value)> Handler;

  ShillServiceObserver(const std::string& service_path,
                       const Handler& handler);

  virtual ~ShillServiceObserver();

  // ShillPropertyChangedObserver overrides.
  virtual void OnPropertyChanged(const std::string& key,
                                 const base::Value& value) OVERRIDE;

 private:
  std::string service_path_;
  Handler handler_;

  DISALLOW_COPY_AND_ASSIGN(ShillServiceObserver);
};

}  // namespace internal
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_SHILL_SERVICE_OBSERVER_H_
