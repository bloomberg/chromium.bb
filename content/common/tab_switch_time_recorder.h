// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_TAB_SWITCH_TIME_RECORDER_H_
#define CONTENT_COMMON_TAB_SWITCH_TIME_RECORDER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace gfx {
struct PresentationFeedback;
}

namespace content {

// Generates UMA metric to track the duration of tab switching from when the
// active tab is changed until the frame presentation time. The metric will be
// separated into two whether the tab switch has saved frames or not.
class CONTENT_EXPORT TabSwitchTimeRecorder {
 public:
  TabSwitchTimeRecorder();
  ~TabSwitchTimeRecorder();

  // Begin recording time, ends when DidPresentFrame is called. Returns a
  // matching callback to end the time recording that will call DidPresentFrame.
  base::OnceCallback<void(const gfx::PresentationFeedback&)> BeginTimeRecording(
      const base::TimeTicks tab_switch_start_time,
      bool has_saved_frames,
      const base::TimeTicks render_widget_visibility_request_timestamp =
          base::TimeTicks());

 private:
  // End the time recording, and upload the result to histogram if it has a
  // correct matching BeginTimeRecording preceding this call.
  void DidPresentFrame(
      bool has_saved_frame,
      base::TimeTicks tab_switch_start_time,
      base::TimeTicks render_widget_visibility_request_timestamp,
      const gfx::PresentationFeedback& feedback);

  base::WeakPtr<TabSwitchTimeRecorder> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<TabSwitchTimeRecorder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabSwitchTimeRecorder);
};

}  // namespace content

#endif  // CONTENT_COMMON_TAB_SWITCH_TIME_RECORDER_H_
