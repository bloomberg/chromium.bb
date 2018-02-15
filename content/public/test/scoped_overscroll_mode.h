// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SCOPED_OVERSCROLL_MODE_H_
#define CONTENT_PUBLIC_TEST_SCOPED_OVERSCROLL_MODE_H_

#include "base/macros.h"
#include "content/public/browser/overscroll_configuration.h"

namespace content {

// Helper class to set the overscroll configuration mode mode temporarily in
// tests.
class ScopedOverscrollMode {
 public:
  explicit ScopedOverscrollMode(OverscrollConfig::Mode mode);
  ~ScopedOverscrollMode();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedOverscrollMode);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SCOPED_OVERSCROLL_MODE_H_
