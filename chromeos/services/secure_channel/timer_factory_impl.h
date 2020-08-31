// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/secure_channel/timer_factory.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace chromeos {

namespace secure_channel {

// Concrete TimerFactory implementation, which returns base::OneShotTimer
// objects.
class TimerFactoryImpl : public TimerFactory {
 public:
  class Factory {
   public:
    static std::unique_ptr<TimerFactory> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<TimerFactory> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  ~TimerFactoryImpl() override;

 private:
  TimerFactoryImpl();

  // TimerFactory:
  std::unique_ptr<base::OneShotTimer> CreateOneShotTimer() override;

  DISALLOW_COPY_AND_ASSIGN(TimerFactoryImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_TIMER_FACTORY_IMPL_H_
