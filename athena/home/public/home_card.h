// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_PUBLIC_HOME_CARD_H_
#define ATHENA_HOME_PUBLIC_HOME_CARD_H_

#include "athena/common/athena_export.h"

namespace app_list {
class SearchProvider;
}

namespace gfx {
class Rect;
}

namespace athena {
class AppModelBuilder;

class ATHENA_EXPORT HomeCard {
 public:
  // Creates/deletes/gets the singleton object of the HomeCard
  // implementation. Takes the ownership of |model_builder|.
  static HomeCard* Create(AppModelBuilder* model_builder);
  static void Shutdown();
  static HomeCard* Get();

  virtual ~HomeCard() {}

  // Registers a search_provider to the HomeCard. Receiver will take
  // the ownership of the specified provider.
  virtual void RegisterSearchProvider(
      app_list::SearchProvider* search_provider) = 0;

  // Called when the virtual keyboard changed has changed to |bounds|.
  virtual void UpdateVirtualKeyboardBounds(
      const gfx::Rect& bounds) = 0;
};

}  // namespace athena

#endif  // ATHENA_HOME_PUBLIC_HOME_CARD_H_
