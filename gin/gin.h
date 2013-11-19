// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_GIN_H_
#define GIN_GIN_H_

#include "base/basictypes.h"

namespace v8 {
class Isolate;
}

namespace gin {

class Gin {
 public:
  Gin();
  ~Gin();

  v8::Isolate* isolate() { return isolate_; }

 private:
  v8::Isolate* isolate_;

 DISALLOW_COPY_AND_ASSIGN(Gin);
};

}  // namespace gin

#endif  // GIN_INITIALIZE_H_
