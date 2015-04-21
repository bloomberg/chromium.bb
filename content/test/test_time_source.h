// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_TIME_SOURCE_H_
#define CONTENT_TEST_TEST_TIME_SOURCE_H_

#include "base/memory/ref_counted.h"
#include "content/child/scheduler/time_source.h"

namespace cc {
class TestNowSource;
}

namespace content {

class TestTimeSource : public TimeSource {
 public:
  explicit TestTimeSource(scoped_refptr<cc::TestNowSource> time_source);
  ~TestTimeSource() override;

  base::TimeTicks Now() const override;

 private:
  scoped_refptr<cc::TestNowSource> time_source_;

  DISALLOW_COPY_AND_ASSIGN(TestTimeSource);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_TIME_SOURCE_H_
