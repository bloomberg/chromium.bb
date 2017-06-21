// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/webrtc/webrtc_webcam_browsertest.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/video_capture/public/cpp/constants.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {

#if defined(OS_WIN)
// These tests are flaky on WebRTC Windows bots: https://crbug.com/633242.
#define MAYBE_GetPhotoCapabilities DISABLED_GetPhotoCapabilities
#define MAYBE_GetPhotoSettings DISABLED_GetPhotoSettings
#define MAYBE_TakePhoto DISABLED_TakePhoto
#define MAYBE_GrabFrame DISABLED_GrabFrame
#define MAYBE_GetTrackCapabilities DISABLED_GetTrackCapabilities
#define MAYBE_GetTrackSettings DISABLED_GetTrackSettings
#define MAYBE_ManipulateZoom DISABLED_ManipulateZoom
#else
#define MAYBE_GetPhotoCapabilities GetPhotoCapabilities
#define MAYBE_GetPhotoSettings GetPhotoSettings
#define MAYBE_TakePhoto TakePhoto
#define MAYBE_GrabFrame GrabFrame
#define MAYBE_GetTrackCapabilities GetTrackCapabilities
#define MAYBE_GetTrackSettings GetTrackSettings
#define MAYBE_ManipulateZoom ManipulateZoom
#endif

namespace {

static const char kImageCaptureHtmlFile[] = "/media/image_capture_test.html";

// TODO(mcasas): enable real-camera tests by disabling the Fake Device for
// platforms where the ImageCaptureCode is landed, https://crbug.com/656810
static struct TargetCamera {
  bool use_fake;
} const kTargetCameras[] = {{true},
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_ANDROID)
                            {false}
#endif
};

static struct TargetVideoCaptureStack {
  bool use_video_capture_service;
} const kTargetVideoCaptureStacks[] = {{false},
// Mojo video capture is currently not supported on Android
// TODO(chfremer): Remove this as soon as https://crbug.com/720500 is
// resolved.
#if !defined(OS_ANDROID)
                                       {true}
#endif
};

}  // namespace

// This class is the content_browsertests for Image Capture API, which allows
// for capturing still images out of a MediaStreamTrack. Is a
// WebRtcWebcamBrowserTest to be able to use a physical camera.
class WebRtcImageCaptureBrowserTestBase : public WebRtcWebcamBrowserTest {
 public:
  WebRtcImageCaptureBrowserTestBase() = default;
  ~WebRtcImageCaptureBrowserTestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcWebcamBrowserTest::SetUpCommandLine(command_line);

    ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));

    // "GetUserMedia": enables navigator.mediaDevices.getUserMedia();
    // TODO(mcasas): remove GetUserMedia after https://crbug.com/503227.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "GetUserMedia");
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    WebRtcWebcamBrowserTest::SetUp();
  }

  // Tries to run a |command| JS test, returning true if the test can be safely
  // skipped or it works as intended, or false otherwise.
  virtual bool RunImageCaptureTestCase(const std::string& command) {
#if defined(OS_ANDROID)
    // TODO(mcasas): fails on Lollipop devices: https://crbug.com/634811
    if (base::android::BuildInfo::GetInstance()->sdk_int() <
        base::android::SDK_VERSION_MARSHMALLOW) {
      return true;
    }
#endif

    GURL url(embedded_test_server()->GetURL(kImageCaptureHtmlFile));
    NavigateToURL(shell(), url);

    if (!IsWebcamAvailableOnSystem(shell()->web_contents())) {
      DVLOG(1) << "No video device; skipping test...";
      return true;
    }

    std::string result;
    if (!ExecuteScriptAndExtractString(shell(), command, &result))
      return false;
    DLOG_IF(ERROR, result != "OK") << result;
    return result == "OK";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcImageCaptureBrowserTestBase);
};

// Test fixture for setting up a capture device (real or fake) that successfully
// serves all image capture requests.
class WebRtcImageCaptureSucceedsBrowserTest
    : public WebRtcImageCaptureBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<TargetCamera, TargetVideoCaptureStack>> {
 public:
  WebRtcImageCaptureSucceedsBrowserTest() = default;
  ~WebRtcImageCaptureSucceedsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcImageCaptureBrowserTestBase::SetUpCommandLine(command_line);

    if (std::get<0>(GetParam()).use_fake) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kUseFakeDeviceForMediaStream);
      ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream));
    }
    if (std::get<1>(GetParam()).use_video_capture_service) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kEnableFeatures, video_capture::kMojoVideoCapture.name);
    }
  }

  bool RunImageCaptureTestCase(const std::string& command) override {
    // TODO(chfremer): Enable test cases using the video capture service with
    // real cameras as soon as root cause for https://crbug.com/733582 is
    // understood and resolved.
    if ((!std::get<0>(GetParam()).use_fake) &&
        (std::get<1>(GetParam()).use_video_capture_service)) {
      LOG(INFO) << "Skipping this test case";
      return true;
    }
    return WebRtcImageCaptureBrowserTestBase::RunImageCaptureTestCase(command);
  }
};

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_GetPhotoCapabilities) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(
      RunImageCaptureTestCase("testCreateAndGetPhotoCapabilitiesSucceeds()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_GetPhotoSettings) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(
      RunImageCaptureTestCase("testCreateAndGetPhotoSettingsSucceeds()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest, MAYBE_TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhotoSucceeds()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest, MAYBE_GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrameSucceeds()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_GetTrackCapabilities) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGetTrackCapabilities()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_GetTrackSettings) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGetTrackSettings()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSucceedsBrowserTest,
                       MAYBE_ManipulateZoom) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testManipulateZoom()"));
}

INSTANTIATE_TEST_CASE_P(
    ,
    WebRtcImageCaptureSucceedsBrowserTest,
    testing::Combine(testing::ValuesIn(kTargetCameras),
                     testing::ValuesIn(kTargetVideoCaptureStacks)));

// Test fixture template for setting up a fake device with a custom
// configuration. We are going to use this to set up fake devices that respond
// to invocation of various ImageCapture API calls with a failure response.
template <typename FakeDeviceConfigTraits>
class WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest
    : public WebRtcImageCaptureBrowserTestBase,
      public testing::WithParamInterface<TargetVideoCaptureStack> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcImageCaptureBrowserTestBase::SetUpCommandLine(command_line);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        std::string("config=") + FakeDeviceConfigTraits::config());
    if (GetParam().use_video_capture_service) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kEnableFeatures, video_capture::kMojoVideoCapture.name);
    }
  }
};

struct GetPhotoStateFailsConfigTraits {
  static std::string config() {
    return media::FakeVideoCaptureDeviceFactory::
        kDeviceConfigForGetPhotoStateFails;
  }
};

using WebRtcImageCaptureGetPhotoStateFailsBrowserTest =
    WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest<
        GetPhotoStateFailsConfigTraits>;

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureGetPhotoStateFailsBrowserTest,
                       GetCapabilities) {
  embedded_test_server()->StartAcceptingConnections();
  // When the fake device faile, we expect an empty set of capabilities to
  // reported back to JS.
  ASSERT_TRUE(
      RunImageCaptureTestCase("testCreateAndGetPhotoCapabilitiesSucceeds()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureGetPhotoStateFailsBrowserTest,
                       TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhotoSucceeds()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureGetPhotoStateFailsBrowserTest,
                       GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrameSucceeds()"));
}

INSTANTIATE_TEST_CASE_P(,
                        WebRtcImageCaptureGetPhotoStateFailsBrowserTest,
                        testing::ValuesIn(kTargetVideoCaptureStacks));

struct SetPhotoOptionsFailsConfigTraits {
  static std::string config() {
    return media::FakeVideoCaptureDeviceFactory::
        kDeviceConfigForSetPhotoOptionsFails;
  }
};

using WebRtcImageCaptureSetPhotoOptionsFailsBrowserTest =
    WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest<
        SetPhotoOptionsFailsConfigTraits>;

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSetPhotoOptionsFailsBrowserTest,
                       TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhotoIsRejected()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureSetPhotoOptionsFailsBrowserTest,
                       GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrameSucceeds()"));
}

INSTANTIATE_TEST_CASE_P(,
                        WebRtcImageCaptureSetPhotoOptionsFailsBrowserTest,
                        testing::ValuesIn(kTargetVideoCaptureStacks));

struct TakePhotoFailsConfigTraits {
  static std::string config() {
    return media::FakeVideoCaptureDeviceFactory::kDeviceConfigForTakePhotoFails;
  }
};

using WebRtcImageCaptureTakePhotoFailsBrowserTest =
    WebRtcImageCaptureCustomConfigFakeDeviceBrowserTest<
        TakePhotoFailsConfigTraits>;

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureTakePhotoFailsBrowserTest, TakePhoto) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndTakePhotoIsRejected()"));
}

IN_PROC_BROWSER_TEST_P(WebRtcImageCaptureTakePhotoFailsBrowserTest, GrabFrame) {
  embedded_test_server()->StartAcceptingConnections();
  ASSERT_TRUE(RunImageCaptureTestCase("testCreateAndGrabFrameSucceeds()"));
}

INSTANTIATE_TEST_CASE_P(,
                        WebRtcImageCaptureTakePhotoFailsBrowserTest,
                        testing::ValuesIn(kTargetVideoCaptureStacks));

}  // namespace content
