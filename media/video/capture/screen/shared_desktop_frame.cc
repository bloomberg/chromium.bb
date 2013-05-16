// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/shared_desktop_frame.h"

#include "base/memory/scoped_ptr.h"

namespace media {

class SharedDesktopFrame::Core : public base::RefCountedThreadSafe<Core> {
 public:
  Core(webrtc::DesktopFrame* frame) : frame_(frame) {}

  webrtc::DesktopFrame* frame() { return frame_.get(); }

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core() {}

  scoped_ptr<webrtc::DesktopFrame> frame_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

SharedDesktopFrame::~SharedDesktopFrame() {}

// static
SharedDesktopFrame* SharedDesktopFrame::Wrap(
    webrtc::DesktopFrame* desktop_frame) {
   return new SharedDesktopFrame(new Core(desktop_frame));
}

webrtc::DesktopFrame* SharedDesktopFrame::GetUnderlyingFrame() {
  return core_->frame();
}

SharedDesktopFrame* SharedDesktopFrame::Share() {
  SharedDesktopFrame* result = new SharedDesktopFrame(core_);
  result->set_dpi(dpi());
  result->set_capture_time_ms(capture_time_ms());
  *result->mutable_updated_region() = updated_region();
  return result;
}

bool SharedDesktopFrame::IsShared() {
  return !core_->HasOneRef();
}

SharedDesktopFrame::SharedDesktopFrame(scoped_refptr<Core> core)
    : DesktopFrame(core->frame()->size(), core->frame()->stride(),
                   core->frame()->data(), core->frame()->shared_memory()),
      core_(core) {
}

}  // namespace media
