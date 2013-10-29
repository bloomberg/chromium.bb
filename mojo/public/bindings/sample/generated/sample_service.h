// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_H_
#define MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_H_

#include "mojo/public/bindings/lib/bindings.h"

namespace sample {
class Foo;

class Service {
 public:
  virtual void Frobinate(const Foo* foo, bool baz, mojo::Handle port) = 0;
};

}  // namespace sample

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_H_
