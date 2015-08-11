// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_MEDIA_LOG_H_
#define MEDIA_BASE_MOCK_MEDIA_LOG_H_

#include "media/base/media_log.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockMediaLog : public MediaLog {
 public:
  MockMediaLog();

  MOCK_METHOD1(DoAddEventLogString, void(const std::string& event));

  // Trampoline method to workaround GMOCK problems with scoped_ptr<>.
  // Also simplifies tests to be able to string match on the log string
  // representation on the added event.
  void AddEvent(scoped_ptr<MediaLogEvent> event) override {
    DoAddEventLogString(MediaEventToLogString(*event));
  }

 protected:
  virtual ~MockMediaLog();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaLog);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_MEDIA_LOG_H_
