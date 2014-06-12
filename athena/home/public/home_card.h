// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_PUBLIC_HOME_CARD_H_
#define ATHENA_HOME_PUBLIC_HOME_CARD_H_

#include "athena/athena_export.h"

namespace athena {
class AppModelBuilder;

class ATHENA_EXPORT HomeCard {
 public:
  // Creates and deletes the singleton object of the HomeCard
  // implementation. Takes the ownership of |model_builder|.
  static HomeCard* Create(AppModelBuilder* model_builder);
  static void Shutdown();

  virtual ~HomeCard() {}
};

}  // namespace athena

#endif  // ATHENA_HOME_PUBLIC_HOME_CARD_H_
