// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_EXCEPTION_H_
#define GIN_EXCEPTION_H_

#include <string>

#include "base/basictypes.h"
#include "v8/include/v8.h"

namespace gin {

class TryCatch {
 public:
  TryCatch();
  ~TryCatch();

  bool HasCaught();
  std::string GetPrettyMessage();

 private:
  v8::TryCatch try_catch_;

  DISALLOW_COPY_AND_ASSIGN(TryCatch);
};

}  // namespace gin

#endif  // GIN_EXCEPTION_H_
