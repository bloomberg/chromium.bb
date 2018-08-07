// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/vr/test/mock_openvr_device_hook_base.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"

#include <memory>

namespace vr {

namespace {

struct Frame {
  device::SubmittedFrameData submitted;
  device::PoseFrameData pose;
  device::DeviceConfig config;
};

class MyOpenVRMock : public MockOpenVRBase {
 public:
  void OnFrameSubmitted(device::SubmittedFrameData frame_data) final;
  device::DeviceConfig WaitGetDeviceConfig() final {
    device::DeviceConfig ret = {
        0.2f /* ipd */,
        {0.1f, 0.2f, 0.3f, 0.4f} /* left projection raw */,
        {0.5f, 0.6f, 0.7f, 0.8f} /* right projection raw */};
    return ret;
  }
  device::PoseFrameData WaitGetPresentingPose() final;
  device::PoseFrameData WaitGetMagicWindowPose() final;

  // The test waits for a submitted frame before returning.
  void WaitForFrames(int count) {
    DCHECK(!wait_loop_);
    wait_frame_count_ = count;

    base::RunLoop* wait_loop =
        new base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed);
    wait_loop_ = wait_loop;
    wait_loop->Run();
    delete wait_loop;
  }

  std::vector<Frame> submitted_frames;
  device::PoseFrameData last_immersive_frame_data = {};

 private:
  // Set to null on background thread after calling Quit(), so we can ensure we
  // only call Quit once.
  base::RunLoop* wait_loop_ = nullptr;

  int wait_frame_count_ = 0;
  int num_frames_submitted_ = 0;

  bool has_last_immersive_frame_data_ = false;
  int frame_id_ = 0;
};

unsigned int ParseColorFrameId(device::Color color) {
  // Corresponding math in test_webxr_poses.html.
  unsigned int frame_id =
      static_cast<unsigned int>(color.r) + 256 * color.g + 256 * 256 * color.b;
  return frame_id;
}

void MyOpenVRMock::OnFrameSubmitted(device::SubmittedFrameData frame_data) {
  unsigned int frame_id = ParseColorFrameId(frame_data.color);
  DLOG(ERROR) << "Frame Submitted: " << num_frames_submitted_ << " "
              << frame_id;
  submitted_frames.push_back(
      {frame_data, last_immersive_frame_data, WaitGetDeviceConfig()});

  num_frames_submitted_++;
  if (num_frames_submitted_ >= wait_frame_count_ && wait_frame_count_ > 0 &&
      wait_loop_) {
    wait_loop_->Quit();
    wait_loop_ = nullptr;
  }

  EXPECT_TRUE(has_last_immersive_frame_data_)
      << "Frame submitted without any frame data provided";

  // We expect a waitGetPoses, then 2 submits (one for each eye), so after 2
  // submitted frames don't use the same frame_data again.
  if (num_frames_submitted_ % 2 == 0)
    has_last_immersive_frame_data_ = false;
}

device::PoseFrameData MyOpenVRMock::WaitGetMagicWindowPose() {
  device::PoseFrameData pose = {};
  pose.is_valid = true;
  // Almost identity matrix - enough different that we can identify if magic
  // window poses are used instead of presenting poses.
  pose.device_to_origin[0] = 1;
  pose.device_to_origin[5] = -1;
  pose.device_to_origin[10] = 1;
  pose.device_to_origin[15] = 1;
  return pose;
}

device::PoseFrameData MyOpenVRMock::WaitGetPresentingPose() {
  DLOG(ERROR) << "WaitGetPresentingPose: " << frame_id_;

  device::PoseFrameData pose = {};
  pose.is_valid = true;
  // Start with identity matrix.
  pose.device_to_origin[0] = 1;
  pose.device_to_origin[5] = 1;
  pose.device_to_origin[10] = 1;
  pose.device_to_origin[15] = 1;

  // Add a translation so each frame gets a different transform, and so its easy
  // to identify what the expected pose is.
  pose.device_to_origin[3] = frame_id_;

  has_last_immersive_frame_data_ = true;
  frame_id_++;
  last_immersive_frame_data = pose;

  return pose;
}

std::string GetMatrixAsString(const float m[]) {
  // Dump the transpose of the matrix due to openvr vs. webxr matrix format
  // differences.
  return base::StringPrintf(
      "[%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f]", m[0],
      m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10], m[14],
      m[3], m[7], m[11], m[15]);
}

std::string GetPoseAsString(const Frame& frame) {
  return GetMatrixAsString(frame.pose.device_to_origin);
}

}  // namespace

// Pixel test for WebVR/WebXR - start presentation, submit frames, get data back
// out. Validates that submitted frames used expected pose.
void TestPresentationPosesImpl(WebXrVrBrowserTestBase* t,
                               std::string filename) {
  MyOpenVRMock my_mock;

  // Load the test page, and enter presentation.
  t->LoadUrlAndAwaitInitialization(t->GetHtmlTestFile(filename));
  t->EnterSessionWithUserGestureOrFail();

  // Wait for JavaScript to submit at least one frame.
  EXPECT_TRUE(
      t->PollJavaScriptBoolean("hasPresentedFrame", t->kPollTimeoutShort))
      << "No frame submitted";

  // Render at least 20 frames.  Make sure each has the right submitted pose.
  my_mock.WaitForFrames(20);

  // Exit presentation.
  t->EndSessionOrFail();

  // Stop hooking OpenVR, so we can safely analyze our cached data without
  // incoming calls (there may be leftover mojo messages queued).
  device::OpenVRDeviceProvider::SetTestHook(nullptr);

  // Analyze the submitted frames - check for a few things:
  // 1. Each frame id should be submitted at most once for each of the left and
  // right eyes.
  // 2. The pose that WebXR used for rendering the submitted frame should be the
  // one that we expected.
  std::set<unsigned int> seen_left;
  std::set<unsigned int> seen_right;
  unsigned int max_frame_id = 0;
  for (auto frame : my_mock.submitted_frames) {
    const device::SubmittedFrameData& data = frame.submitted;

    // The test page encodes the frame id as the clear color.
    unsigned int frame_id = ParseColorFrameId(data.color);

    // Validate that each frame is only seen once for each eye.
    DLOG(ERROR) << "Frame id: " << frame_id;
    if (data.left_eye) {
      EXPECT_TRUE(seen_left.find(frame_id) == seen_left.end())
          << "Frame for left eye submitted more than once";
      seen_left.insert(frame_id);
    } else {
      EXPECT_TRUE(seen_right.find(frame_id) == seen_right.end())
          << "Frame for right eye submitted more than once";
      seen_right.insert(frame_id);
    }

    // Validate that frames arrive in order.
    EXPECT_TRUE(frame_id >= max_frame_id) << "Frame received out of order";
    max_frame_id = std::max(frame_id, max_frame_id);

    // Validate that the JavaScript-side cache of frames contains our submitted
    // frame.
    EXPECT_TRUE(t->RunJavaScriptAndExtractBoolOrFail(
        base::StringPrintf("checkFrameOccurred(%d)", frame_id)))
        << "JavaScript-side frame cache does not contain submitted frame";

    // Validate that the JavaScript-side cache of frames has the correct pose.
    EXPECT_TRUE(t->RunJavaScriptAndExtractBoolOrFail(base::StringPrintf(
        "checkFramePose(%d, %s)", frame_id, GetPoseAsString(frame).c_str())))
        << "JavaScript-side frame cache has incorrect pose";
  }

  // Tell JavaScript that it is done with the test.
  t->ExecuteStepAndWait("finishTest()");
  t->EndTest();
}

IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard,
                       REQUIRES_GPU(TestPresentationPoses)) {
  TestPresentationPosesImpl(this, "test_webxr_poses");
}

}  // namespace vr
