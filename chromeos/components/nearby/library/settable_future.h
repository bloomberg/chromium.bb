// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SETTABLE_FUTURE_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SETTABLE_FUTURE_H_

#include "chromeos/components/nearby/library/future.h"

namespace location {
namespace nearby {

// A SettableFuture is a type of Future whose result can be set.
//
// https://google.github.io/guava/releases/20.0/api/docs/com/google/common/util/concurrent/SettableFuture.html
template <typename T>
class SettableFuture : public Future<T> {
 public:
  virtual ~SettableFuture() {}

  virtual bool set(T value) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SETTABLE_FUTURE_H_
