// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_SESSION_UMA_HELPER_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_SESSION_UMA_HELPER_H_

#include "base/memory/scoped_ptr.h"
#include "base/time/clock.h"
#include "content/common/content_export.h"

namespace base {
class Clock;
}  // base namespace

namespace content {

class CONTENT_EXPORT MediaSessionUmaHelper {
 public:
  // This is used for UMA histogram (Media.Session.Suspended). New values should
  // be appended only and must be added before |Count|.
  enum class MediaSessionSuspendedSource {
    SystemTransient = 0,
    SystemPermanent = 1,
    UI = 2,
    CONTENT = 3,
    Count // Leave at the end.
  };

  MediaSessionUmaHelper();
  ~MediaSessionUmaHelper();

  void RecordSessionSuspended(MediaSessionSuspendedSource source) const;

  // Record the result of calling the native requestAudioFocus().
  void RecordRequestAudioFocusResult(bool result) const;

  void OnSessionActive();
  void OnSessionSuspended();
  void OnSessionInactive();

  void SetClockForTest(scoped_ptr<base::Clock> testing_clock);

 private:
  base::TimeDelta total_active_time_;
  base::Time current_active_time_;
  scoped_ptr<base::Clock> clock_;
};

}  // namespace content

#endif // CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_SESSION_UMA_HELPER_H_
