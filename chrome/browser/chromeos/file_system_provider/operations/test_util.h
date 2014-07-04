// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_TEST_UTIL_H_

#include <vector>

#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"

namespace extensions {
struct Event;
}  // namespace extensions

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace util {

// Fake event dispatcher implementation with extra logging capability. Acts as
// a providing extension end-point.
class LoggingDispatchEventImpl {
 public:
  explicit LoggingDispatchEventImpl(bool dispatch_reply);
  virtual ~LoggingDispatchEventImpl();

  // Handles sending an event to a providing extension.
  bool OnDispatchEventImpl(scoped_ptr<extensions::Event> event);

  // Returns events sent to providing extensions.
  ScopedVector<extensions::Event>& events() { return events_; }

 private:
  ScopedVector<extensions::Event> events_;
  bool dispatch_reply_;

  DISALLOW_COPY_AND_ASSIGN(LoggingDispatchEventImpl);
};

// Container for remembering operations' callback invocations.
typedef std::vector<base::File::Error> StatusCallbackLog;

// Pushes a result of the StatusCallback invocation to a log vector.
void LogStatusCallback(StatusCallbackLog* log, base::File::Error result);

}  // namespace util
}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_OPERATIONS_TEST_UTIL_H_
