// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/create_video_capture_device_factory.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <mfcaptureengine.h>
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"  // For fine-grained suppression.
#include "media/capture/video/win/video_capture_device_factory_win.h"
#include "media/capture/video/win/video_capture_device_mf_win.h"
#endif

#if defined(OS_MACOSX)
#include "media/capture/video/mac/video_capture_device_factory_mac.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "media/capture/video/android/video_capture_device_android.h"
#include "media/capture/video/android/video_capture_device_factory_android.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/local_gpu_memory_buffer_manager.h"
#include "media/capture/video/chromeos/public/cros_features.h"
#include "media/capture/video/chromeos/video_capture_device_chromeos_halv3.h"
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"
#endif

#if defined(OS_MACOSX)
// Mac will always give you the size you ask for and this case will fail.
#define MAYBE_UsingRealWebcam_AllocateBadSize \
  DISABLED_UsingRealWebcam_AllocateBadSize
// We will always get YUYV from the Mac AVFoundation implementations.
#define MAYBE_UsingRealWebcam_CaptureMjpeg DISABLED_UsingRealWebcam_CaptureMjpeg
// TODO(crbug.com/923874).
#define MAYBE_UsingRealWebcam_TakePhoto DISABLED_UsingRealWebcam_TakePhoto
  // TODO(crbug.com/923874).
#define MAYBE_UsingRealWebcam_GetPhotoState DISABLED_UsingRealWebcam_GetPhotoState
  // TODO(crbug.com/923874).
#define MAYBE_UsingRealWebcam_CaptureWithSize DISABLED_UsingRealWebcam_CaptureWithSize
#define MAYBE_UsingRealWebcam_CheckPhotoCallbackRelease \
  UsingRealWebcam_CheckPhotoCallbackRelease
#elif defined(OS_WIN)
// TODO(crbug.com/893494): Fails on win: error: Value of: device_descriptor.
#define MAYBE_UsingRealWebcam_AllocateBadSize \
  DISABLED_UsingRealWebcam_AllocateBadSize
#define MAYBE_UsingRealWebcam_CaptureMjpeg DISABLED_UsingRealWebcam_CaptureMjpeg
#define MAYBE_UsingRealWebcam_TakePhoto DISABLED_UsingRealWebcam_TakePhoto
#define MAYBE_UsingRealWebcam_GetPhotoState \
  DISABLED_UsingRealWebcam_GetPhotoState
#define MAYBE_UsingRealWebcam_CaptureWithSize \
  DISABLED_UsingRealWebcam_CaptureWithSize
#define MAYBE_UsingRealWebcam_CheckPhotoCallbackRelease \
  DISABLED_UsingRealWebcam_CheckPhotoCallbackRelease
#elif defined(OS_ANDROID)
#define MAYBE_UsingRealWebcam_AllocateBadSize UsingRealWebcam_AllocateBadSize
#define MAYBE_UsingRealWebcam_CaptureMjpeg UsingRealWebcam_CaptureMjpeg
#define MAYBE_UsingRealWebcam_TakePhoto UsingRealWebcam_TakePhoto
#define MAYBE_UsingRealWebcam_GetPhotoState UsingRealWebcam_GetPhotoState
#define MAYBE_UsingRealWebcam_CaptureWithSize UsingRealWebcam_CaptureWithSize
#define MAYBE_UsingRealWebcam_CheckPhotoCallbackRelease \
  UsingRealWebcam_CheckPhotoCallbackRelease
#elif defined(OS_CHROMEOS)
#define MAYBE_UsingRealWebcam_AllocateBadSize \
  DISABLED_UsingRealWebcam_AllocateBadSize
#define MAYBE_UsingRealWebcam_CaptureMjpeg UsingRealWebcam_CaptureMjpeg
#define MAYBE_UsingRealWebcam_TakePhoto UsingRealWebcam_TakePhoto
#define MAYBE_UsingRealWebcam_GetPhotoState UsingRealWebcam_GetPhotoState
#define MAYBE_UsingRealWebcam_CaptureWithSize UsingRealWebcam_CaptureWithSize
#define MAYBE_UsingRealWebcam_CheckPhotoCallbackRelease \
  UsingRealWebcam_CheckPhotoCallbackRelease
#elif defined(OS_LINUX)
// UsingRealWebcam_AllocateBadSize will hang when a real camera is attached and
// if more than one test is trying to use the camera (even across processes). Do
// NOT renable this test without fixing the many bugs associated with it:
// http://crbug.com/94134 http://crbug.com/137260 http://crbug.com/417824
#define MAYBE_UsingRealWebcam_AllocateBadSize \
  DISABLED_UsingRealWebcam_AllocateBadSize
#define MAYBE_UsingRealWebcam_CaptureMjpeg UsingRealWebcam_CaptureMjpeg
#define MAYBE_UsingRealWebcam_TakePhoto UsingRealWebcam_TakePhoto
#define MAYBE_UsingRealWebcam_GetPhotoState UsingRealWebcam_GetPhotoState
#define MAYBE_UsingRealWebcam_CaptureWithSize UsingRealWebcam_CaptureWithSize
#define MAYBE_UsingRealWebcam_CheckPhotoCallbackRelease \
  UsingRealWebcam_CheckPhotoCallbackRelease
#else
#define MAYBE_UsingRealWebcam_AllocateBadSize UsingRealWebcam_AllocateBadSize
#define MAYBE_UsingRealWebcam_CaptureMjpeg UsingRealWebcam_CaptureMjpeg
#define MAYBE_UsingRealWebcam_TakePhoto DISABLED_UsingRealWebcam_TakePhoto
#define MAYBE_UsingRealWebcam_GetPhotoState \
  DISABLED_UsingRealWebcam_GetPhotoState
#define MAYBE_UsingRealWebcam_CaptureWithSize UsingRealWebcam_CaptureWithSize
#define MAYBE_UsingRealWebcam_CheckPhotoCallbackRelease \
  UsingRealWebcam_CheckPhotoCallbackRelease
#endif

// Wrap the TEST_P macro into another one to allow to preprocess |test_name|
// macros. Needed until https://github.com/google/googletest/issues/389 is
// fixed.
#define WRAPPED_TEST_P(test_case_name, test_name) \
  TEST_P(test_case_name, test_name)

using ::testing::_;
using ::testing::Invoke;
using ::testing::SaveArg;
using ::testing::Return;

namespace media {
namespace {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

void DumpError(media::VideoCaptureError,
               const base::Location& location,
               const std::string& message) {
  DPLOG(ERROR) << location.ToString() << " " << message;
}

enum VideoCaptureImplementationTweak {
  NONE,
#if defined(OS_WIN)
  WIN_MEDIA_FOUNDATION
#endif
};

#if defined(OS_WIN)
class MockMFPhotoCallback final : public IMFCaptureEngineOnSampleCallback {
 public:
  ~MockMFPhotoCallback() {}

  MOCK_METHOD2(DoQueryInterface, HRESULT(REFIID, void**));
  MOCK_METHOD0(DoAddRef, ULONG(void));
  MOCK_METHOD0(DoRelease, ULONG(void));
  MOCK_METHOD1(DoOnSample, HRESULT(IMFSample*));

  STDMETHOD(QueryInterface)(REFIID riid, void** object) override {
    return DoQueryInterface(riid, object);
  }

  STDMETHOD_(ULONG, AddRef)() override { return DoAddRef(); }

  STDMETHOD_(ULONG, Release)() override { return DoRelease(); }

  STDMETHOD(OnSample)(IMFSample* sample) override { return DoOnSample(sample); }
};
#endif

class MockImageCaptureClient
    : public base::RefCountedThreadSafe<MockImageCaptureClient> {
 public:
  // GMock doesn't support move-only arguments, so we use this forward method.
  void DoOnPhotoTaken(mojom::BlobPtr blob) {
    if (strcmp("image/jpeg", blob->mime_type.c_str()) == 0) {
      ASSERT_GT(blob->data.size(), 4u);
      // Check some bytes that univocally identify |data| as a JPEG File.
      // The first two bytes must be the SOI marker.
      // The next two bytes must be a marker, such as APPn, DTH etc.
      // cf. Section B.2 at https://www.w3.org/Graphics/JPEG/itu-t81.pdf
      EXPECT_EQ(0xFF, blob->data[0]);         // First SOI byte
      EXPECT_EQ(0xD8, blob->data[1]);         // Second SOI byte
      EXPECT_EQ(0xFF, blob->data[2]);         // First byte of the next marker
      OnCorrectPhotoTaken();
    } else if (strcmp("image/png", blob->mime_type.c_str()) == 0) {
      ASSERT_GT(blob->data.size(), 4u);
      EXPECT_EQ('P', blob->data[1]);
      EXPECT_EQ('N', blob->data[2]);
      EXPECT_EQ('G', blob->data[3]);
      OnCorrectPhotoTaken();
    } else {
      ADD_FAILURE() << "Photo format should be jpeg or png";
    }
  }
  MOCK_METHOD0(OnCorrectPhotoTaken, void(void));

  // GMock doesn't support move-only arguments, so we use this forward method.
  void DoOnGetPhotoState(mojom::PhotoStatePtr state) {
    state_ = std::move(state);
    OnCorrectGetPhotoState();
  }
  MOCK_METHOD0(OnCorrectGetPhotoState, void(void));

  const mojom::PhotoState* capabilities() { return state_.get(); }

 private:
  friend class base::RefCountedThreadSafe<MockImageCaptureClient>;
  virtual ~MockImageCaptureClient() = default;

  mojom::PhotoStatePtr state_;
};

}  // namespace

class VideoCaptureDeviceTest
    : public testing::TestWithParam<
          std::tuple<gfx::Size, VideoCaptureImplementationTweak>> {
 public:
#if defined(OS_WIN)
  scoped_refptr<IMFCaptureEngineOnSampleCallback> CreateMockPhotoCallback(
      MockMFPhotoCallback* mock_photo_callback,
      VideoCaptureDevice::TakePhotoCallback callback,
      VideoCaptureFormat format) {
    return scoped_refptr<IMFCaptureEngineOnSampleCallback>(mock_photo_callback);
  }
#endif

  void RunOpenInvalidDeviceTestCase();
  void RunCaptureWithSizeTestCase();
  void RunAllocateBadSizeTestCase();
  void RunReAllocateCameraTestCase();
  void RunCaptureMjpegTestCase();
  void RunNoCameraSupportsPixelFormatMaxTestCase();
  void RunTakePhotoTestCase();
  void RunGetPhotoStateTestCase();

 protected:
  typedef VideoCaptureDevice::Client Client;

  VideoCaptureDeviceTest()
      :
#if defined(OS_MACOSX)
        // Video capture code on MacOSX must run on a CFRunLoop enabled thread
        // for interaction with AVFoundation.
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
#endif
        device_descriptors_(new VideoCaptureDeviceDescriptors()),
        main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        video_capture_client_(CreateDeviceClient()),
        image_capture_client_(new MockImageCaptureClient()) {
#if defined(OS_CHROMEOS)
    local_gpu_memory_buffer_manager_ =
        std::make_unique<LocalGpuMemoryBufferManager>();
    dbus_setter_ = chromeos::DBusThreadManager::GetSetterForTesting();
    VideoCaptureDeviceFactoryChromeOS::SetGpuBufferManager(
        local_gpu_memory_buffer_manager_.get());
    if (!CameraHalDispatcherImpl::GetInstance()->IsStarted()) {
      CameraHalDispatcherImpl::GetInstance()->Start(
          base::DoNothing::Repeatedly<
              media::mojom::JpegDecodeAcceleratorRequest>(),
          base::DoNothing::Repeatedly<
              media::mojom::JpegEncodeAcceleratorRequest>());
    }
#endif
    video_capture_device_factory_ =
        CreateVideoCaptureDeviceFactory(base::ThreadTaskRunnerHandle::Get());
  }

  void SetUp() override {
#if defined(OS_CHROMEOS)
    dbus_setter_->SetPowerManagerClient(
        std::make_unique<chromeos::FakePowerManagerClient>());
#endif
#if defined(OS_ANDROID)
    static_cast<VideoCaptureDeviceFactoryAndroid*>(
        video_capture_device_factory_.get())
        ->ConfigureForTesting();
#elif defined(OS_WIN)
    static_cast<VideoCaptureDeviceFactoryWin*>(
        video_capture_device_factory_.get())
        ->set_use_media_foundation_for_testing(UseWinMediaFoundation());
#endif
  }

#if defined(OS_WIN)
  bool UseWinMediaFoundation() {
    return std::get<1>(GetParam()) == WIN_MEDIA_FOUNDATION;
  }
#endif

  std::unique_ptr<MockVideoCaptureDeviceClient> CreateDeviceClient() {
    auto result = std::make_unique<MockVideoCaptureDeviceClient>();
    ON_CALL(*result, OnError(_, _, _)).WillByDefault(Invoke(DumpError));
    EXPECT_CALL(*result, ReserveOutputBuffer(_, _, _, _)).Times(0);
    EXPECT_CALL(*result, DoOnIncomingCapturedBuffer(_, _, _, _)).Times(0);
    EXPECT_CALL(*result, DoOnIncomingCapturedBufferExt(_, _, _, _, _, _))
        .Times(0);
    ON_CALL(*result, OnIncomingCapturedData(_, _, _, _, _, _, _))
        .WillByDefault(
            Invoke([this](const uint8_t* data, int length,
                          const media::VideoCaptureFormat& frame_format, int,
                          base::TimeTicks, base::TimeDelta, int) {
              ASSERT_GT(length, 0);
              ASSERT_TRUE(data);
              main_thread_task_runner_->PostTask(
                  FROM_HERE,
                  base::BindOnce(&VideoCaptureDeviceTest::OnFrameCaptured,
                                 base::Unretained(this), frame_format));
            }));
    ON_CALL(*result, OnIncomingCapturedGfxBuffer(_, _, _, _, _, _))
        .WillByDefault(Invoke([this](
                                  gfx::GpuMemoryBuffer* buffer,
                                  const media::VideoCaptureFormat& frame_format,
                                  int, base::TimeTicks, base::TimeDelta, int) {
          ASSERT_TRUE(buffer);
          ASSERT_GT(buffer->GetSize().width() * buffer->GetSize().height(), 0);
          main_thread_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&VideoCaptureDeviceTest::OnFrameCaptured,
                             base::Unretained(this), frame_format));
        }));
    return result;
  }

  void OnFrameCaptured(const VideoCaptureFormat& format) {
    last_format_ = format;
    if (run_loop_)
      run_loop_->QuitClosure().Run();
  }

  void WaitForCapturedFrame() {
    run_loop_.reset(
        new base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed));
    run_loop_->Run();
  }

  std::unique_ptr<VideoCaptureDeviceDescriptor> FindUsableDeviceDescriptor() {
    video_capture_device_factory_->GetDeviceDescriptors(
        device_descriptors_.get());

#if defined(OS_ANDROID)
    for (const auto& descriptor : *device_descriptors_) {
      // Android deprecated/legacy devices capture on a single thread, which is
      // occupied by the tests, so nothing gets actually delivered.
      // TODO(mcasas): use those devices' test mode to deliver frames in a
      // background thread, https://crbug.com/626857
      if (!VideoCaptureDeviceFactoryAndroid::IsLegacyOrDeprecatedDevice(
              descriptor.device_id)) {
        DLOG(INFO) << "Using camera " << descriptor.GetNameAndModel();
        return std::make_unique<VideoCaptureDeviceDescriptor>(descriptor);
      }
    }
    DLOG(WARNING) << "No usable camera found";
    return nullptr;
#endif

    if (device_descriptors_->empty()) {
      DLOG(WARNING) << "No camera found";
      return nullptr;
    }
#if defined(OS_WIN)
    // Dump the camera model to help debugging.
    // TODO(alaoui.rda@gmail.com): remove after http://crbug.com/730068 is
    // fixed.
    LOG(INFO) << "Using camera "
              << device_descriptors_->front().GetNameAndModel();
#else
    DLOG(INFO) << "Using camera "
               << device_descriptors_->front().GetNameAndModel();
#endif

    return std::make_unique<VideoCaptureDeviceDescriptor>(
        device_descriptors_->front());
  }

  const VideoCaptureFormat& last_format() const { return last_format_; }

  std::unique_ptr<VideoCaptureDeviceDescriptor>
  GetFirstDeviceDescriptorSupportingPixelFormat(
      const VideoPixelFormat& pixel_format) {
    if (!FindUsableDeviceDescriptor())
      return nullptr;

    for (const auto& descriptor : *device_descriptors_) {
      VideoCaptureFormats supported_formats;
      video_capture_device_factory_->GetSupportedFormats(descriptor,
                                                         &supported_formats);
      for (const auto& formats_iterator : supported_formats) {
        if (formats_iterator.pixel_format == pixel_format) {
          return std::unique_ptr<VideoCaptureDeviceDescriptor>(
              new VideoCaptureDeviceDescriptor(descriptor));
        }
      }
    }
    DVLOG_IF(1, pixel_format != PIXEL_FORMAT_MAX)
        << VideoPixelFormatToString(pixel_format);
    return std::unique_ptr<VideoCaptureDeviceDescriptor>();
  }

  bool IsCaptureSizeSupported(const VideoCaptureDeviceDescriptor& device,
                              const gfx::Size& size) {
    VideoCaptureFormats supported_formats;
    video_capture_device_factory_->GetSupportedFormats(device,
                                                       &supported_formats);
    const auto it = std::find_if(
        supported_formats.begin(), supported_formats.end(),
        [&size](VideoCaptureFormat const& f) { return f.frame_size == size; });
    if (it == supported_formats.end()) {
      DVLOG(1) << "Size " << size.ToString() << " is not supported.";
      return false;
    }
    return true;
  }

  gfx::Size GetSupportedCaptureSize(
      const VideoCaptureDeviceDescriptor& device) {
    VideoCaptureFormats supported_formats;
    video_capture_device_factory_->GetSupportedFormats(device,
                                                       &supported_formats);
    if (supported_formats.size() == 0)
      return gfx::Size(0, 0);

    return supported_formats.begin()->frame_size;
  }

  void RunTestCase(base::OnceClosure test_case) {
#if defined(OS_MACOSX)
    // In order to make the test case run on the actual message loop that has
    // been created for this thread, we need to run it inside a RunLoop. This is
    // required, because on MacOS the capture code must run on a CFRunLoop
    // enabled message loop.
    base::RunLoop run_loop;
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::RunLoop* run_loop, base::OnceClosure* test_case) {
              std::move(*test_case).Run();
              run_loop->Quit();
            },
            &run_loop, &test_case));
    run_loop.Run();
#else
    std::move(test_case).Run();
#endif
  }

#if defined(OS_WIN)
  base::win::ScopedCOMInitializer initialize_com_;
#endif
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors_;
  std::unique_ptr<base::RunLoop> run_loop_;
  scoped_refptr<base::TaskRunner> main_thread_task_runner_;
  std::unique_ptr<MockVideoCaptureDeviceClient> video_capture_client_;
  const scoped_refptr<MockImageCaptureClient> image_capture_client_;
  VideoCaptureFormat last_format_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<LocalGpuMemoryBufferManager> local_gpu_memory_buffer_manager_;
  std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter_;
#endif
  std::unique_ptr<VideoCaptureDeviceFactory> video_capture_device_factory_;
};

// Cause hangs on Windows Debug. http://crbug.com/417824
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_OpenInvalidDevice DISABLED_OpenInvalidDevice
#else
#define MAYBE_OpenInvalidDevice OpenInvalidDevice
#endif
// Tries to allocate an invalid device and verifies it doesn't work.
WRAPPED_TEST_P(VideoCaptureDeviceTest, MAYBE_OpenInvalidDevice) {
  RunTestCase(
      base::BindOnce(&VideoCaptureDeviceTest::RunOpenInvalidDeviceTestCase,
                     base::Unretained(this)));
}
void VideoCaptureDeviceTest::RunOpenInvalidDeviceTestCase() {
  VideoCaptureDeviceDescriptor invalid_descriptor;
  invalid_descriptor.device_id = "jibberish";
  invalid_descriptor.set_display_name("jibberish");
#if defined(OS_WIN)
  invalid_descriptor.capture_api =
      VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation()
          ? VideoCaptureApi::WIN_MEDIA_FOUNDATION
          : VideoCaptureApi::WIN_DIRECT_SHOW;
#elif defined(OS_MACOSX)
  invalid_descriptor.capture_api = VideoCaptureApi::MACOSX_AVFOUNDATION;
#endif
  std::unique_ptr<VideoCaptureDevice> device =
      video_capture_device_factory_->CreateDevice(invalid_descriptor);

#if !defined(OS_MACOSX)
  EXPECT_FALSE(device);
#else
  // The presence of the actual device is only checked on AllocateAndStart()
  // and not on creation.
  EXPECT_CALL(*video_capture_client_, OnError(_, _, _)).Times(1);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));
  device->StopAndDeAllocate();
#endif
}

// See crbug.com/805411.
TEST(VideoCaptureDeviceDescriptor, RemoveTrailingWhitespaceFromDisplayName) {
  VideoCaptureDeviceDescriptor descriptor;
  descriptor.set_display_name("My WebCam\n");
  EXPECT_EQ(descriptor.display_name(), "My WebCam");
}

// Allocates the first enumerated device, and expects a frame.
WRAPPED_TEST_P(VideoCaptureDeviceTest, MAYBE_UsingRealWebcam_CaptureWithSize) {
  RunTestCase(
      base::BindOnce(&VideoCaptureDeviceTest::RunCaptureWithSizeTestCase,
                     base::Unretained(this)));
}
void VideoCaptureDeviceTest::RunCaptureWithSizeTestCase() {
  const auto descriptor = FindUsableDeviceDescriptor();
  ASSERT_TRUE(descriptor);

  const gfx::Size& size = std::get<0>(GetParam());
  if (!IsCaptureSizeSupported(*descriptor, size))
    return;
  const int width = size.width();
  const int height = size.height();

  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(
          device_descriptors_->front()));
  ASSERT_TRUE(device);

  EXPECT_CALL(*video_capture_client_, OnError(_, _, _)).Times(0);
  EXPECT_CALL(*video_capture_client_, OnStarted());

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(width, height);
  capture_params.requested_format.frame_rate = 30.0f;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));

  WaitForCapturedFrame();
  EXPECT_EQ(last_format().frame_size.width(), width);
  EXPECT_EQ(last_format().frame_size.height(), height);
  if (last_format().pixel_format != PIXEL_FORMAT_MJPEG)
    EXPECT_EQ(size.GetArea(), last_format().frame_size.GetArea());
  EXPECT_EQ(last_format().frame_rate, 30);
  device->StopAndDeAllocate();
}

const gfx::Size kCaptureSizes[] = {gfx::Size(640, 480), gfx::Size(1280, 720)};
const VideoCaptureImplementationTweak kCaptureImplementationTweaks[] = {
    NONE,
#if defined(OS_WIN)
    WIN_MEDIA_FOUNDATION
#endif
};

INSTANTIATE_TEST_CASE_P(
    VideoCaptureDeviceTests,
    VideoCaptureDeviceTest,
    testing::Combine(testing::ValuesIn(kCaptureSizes),
                     testing::ValuesIn(kCaptureImplementationTweaks)));

// Allocates a device with an uncommon resolution and verifies frames are
// captured in a close, much more typical one.
WRAPPED_TEST_P(VideoCaptureDeviceTest, MAYBE_UsingRealWebcam_AllocateBadSize) {
  RunTestCase(
      base::BindOnce(&VideoCaptureDeviceTest::RunAllocateBadSizeTestCase,
                     base::Unretained(this)));
}
void VideoCaptureDeviceTest::RunAllocateBadSizeTestCase() {
  const auto descriptor = FindUsableDeviceDescriptor();
  ASSERT_TRUE(descriptor);

  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(*descriptor));
  ASSERT_TRUE(device);

  EXPECT_CALL(*video_capture_client_, OnError(_, _, _)).Times(0);
  EXPECT_CALL(*video_capture_client_, OnStarted());

  const gfx::Size input_size(640, 480);
  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(637, 472);
  capture_params.requested_format.frame_rate = 35;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));
  WaitForCapturedFrame();
  device->StopAndDeAllocate();
  EXPECT_EQ(last_format().frame_size.width(), input_size.width());
  EXPECT_EQ(last_format().frame_size.height(), input_size.height());
  if (last_format().pixel_format != PIXEL_FORMAT_MJPEG)
    EXPECT_EQ(input_size.GetArea(), last_format().frame_size.GetArea());
}

// Cause hangs on Windows, Linux. Fails Android. https://crbug.com/417824
WRAPPED_TEST_P(VideoCaptureDeviceTest,
               DISABLED_UsingRealWebcam_ReAllocateCamera) {
  RunTestCase(
      base::BindOnce(&VideoCaptureDeviceTest::RunReAllocateCameraTestCase,
                     base::Unretained(this)));
}
void VideoCaptureDeviceTest::RunReAllocateCameraTestCase() {
  const auto descriptor = FindUsableDeviceDescriptor();
  ASSERT_TRUE(descriptor);

  // First, do a number of very fast device start/stops.
  for (int i = 0; i <= 5; i++) {
    video_capture_client_ = CreateDeviceClient();
    std::unique_ptr<VideoCaptureDevice> device(
        video_capture_device_factory_->CreateDevice(*descriptor));
    gfx::Size resolution;
    if (i % 2)
      resolution = gfx::Size(640, 480);
    else
      resolution = gfx::Size(1280, 1024);

    VideoCaptureParams capture_params;
    capture_params.requested_format.frame_size = resolution;
    capture_params.requested_format.frame_rate = 30;
    capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
    device->AllocateAndStart(capture_params, std::move(video_capture_client_));
    device->StopAndDeAllocate();
  }

  // Finally, do a device start and wait for it to finish.
  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(320, 240);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;

  video_capture_client_ = CreateDeviceClient();
  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(
          device_descriptors_->front()));

  device->AllocateAndStart(capture_params, std::move(video_capture_client_));
  WaitForCapturedFrame();
  device->StopAndDeAllocate();
  device.reset();
  EXPECT_EQ(last_format().frame_size.width(), 320);
  EXPECT_EQ(last_format().frame_size.height(), 240);
}

// Starts the camera in 720p to try and capture MJPEG format.
WRAPPED_TEST_P(VideoCaptureDeviceTest, MAYBE_UsingRealWebcam_CaptureMjpeg) {
  RunTestCase(base::BindOnce(&VideoCaptureDeviceTest::RunCaptureMjpegTestCase,
                             base::Unretained(this)));
}
void VideoCaptureDeviceTest::RunCaptureMjpegTestCase() {
#if defined(OS_CHROMEOS)
  if (media::ShouldUseCrosCameraService()) {
    VLOG(1)
        << "Skipped on Chrome OS device where HAL v3 camera service is used";
    return;
  }
#endif
  std::unique_ptr<VideoCaptureDeviceDescriptor> device_descriptor =
      GetFirstDeviceDescriptorSupportingPixelFormat(PIXEL_FORMAT_MJPEG);
  ASSERT_TRUE(device_descriptor);

#if defined(OS_WIN)
  base::win::Version version = base::win::GetVersion();
  if (version >= base::win::VERSION_WIN10) {
    VLOG(1) << "Skipped on Win10: http://crbug.com/570604, current: "
            << static_cast<int>(version);
    return;
  }
#endif
  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(*device_descriptor));
  ASSERT_TRUE(device);

  EXPECT_CALL(*video_capture_client_, OnError(_, _, _)).Times(0);
  EXPECT_CALL(*video_capture_client_, OnStarted());

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(1280, 720);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_MJPEG;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));

  WaitForCapturedFrame();
  // Verify we get MJPEG from the device. Not all devices can capture 1280x720
  // @ 30 fps, so we don't care about the exact resolution we get.
  EXPECT_EQ(last_format().pixel_format, PIXEL_FORMAT_MJPEG);
  EXPECT_GE(static_cast<size_t>(1280 * 720),
            last_format().ImageAllocationSize());
  device->StopAndDeAllocate();
}

WRAPPED_TEST_P(VideoCaptureDeviceTest, NoCameraSupportsPixelFormatMax) {
  RunTestCase(base::BindOnce(
      &VideoCaptureDeviceTest::RunNoCameraSupportsPixelFormatMaxTestCase,
      base::Unretained(this)));
}
void VideoCaptureDeviceTest::RunNoCameraSupportsPixelFormatMaxTestCase() {
  // Use PIXEL_FORMAT_MAX to iterate all device names for testing
  // GetDeviceSupportedFormats().
  std::unique_ptr<VideoCaptureDeviceDescriptor> device_descriptor =
      GetFirstDeviceDescriptorSupportingPixelFormat(PIXEL_FORMAT_MAX);
  // Verify no camera returned for PIXEL_FORMAT_MAX. Nothing else to test here
  // since we cannot forecast the hardware capabilities.
  ASSERT_FALSE(device_descriptor);
}

// Starts the camera and verifies that a photo can be taken. The correctness of
// the photo is enforced by MockImageCaptureClient.
WRAPPED_TEST_P(VideoCaptureDeviceTest, MAYBE_UsingRealWebcam_TakePhoto) {
  RunTestCase(base::BindOnce(&VideoCaptureDeviceTest::RunTakePhotoTestCase,
                             base::Unretained(this)));
}
void VideoCaptureDeviceTest::RunTakePhotoTestCase() {
  const auto descriptor = FindUsableDeviceDescriptor();
  ASSERT_TRUE(descriptor);

  const gfx::Size frame_size = GetSupportedCaptureSize(*descriptor);
  if (frame_size == gfx::Size(0, 0))
    return;

#if defined(OS_ANDROID)
  // TODO(mcasas): fails on Lollipop devices, reconnect https://crbug.com/646840
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_MARSHMALLOW) {
    return;
  }
#endif

  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(*descriptor));
  ASSERT_TRUE(device);

  EXPECT_CALL(*video_capture_client_, OnError(_, _, _)).Times(0);
  EXPECT_CALL(*video_capture_client_, OnStarted()).Times(testing::AtLeast(1));

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size = frame_size;
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));

  VideoCaptureDevice::TakePhotoCallback scoped_callback = base::BindOnce(
      &MockImageCaptureClient::DoOnPhotoTaken, image_capture_client_);

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::Closure quit_closure = BindToCurrentLoop(run_loop.QuitClosure());
  EXPECT_CALL(*image_capture_client_.get(), OnCorrectPhotoTaken())
      .Times(1)
      .WillOnce(RunClosure(quit_closure));

  device->TakePhoto(std::move(scoped_callback));
  run_loop.Run();

  device->StopAndDeAllocate();
}

// Starts the camera and verifies that the photo capabilities can be retrieved.
WRAPPED_TEST_P(VideoCaptureDeviceTest, MAYBE_UsingRealWebcam_GetPhotoState) {
  RunTestCase(base::BindOnce(&VideoCaptureDeviceTest::RunGetPhotoStateTestCase,
                             base::Unretained(this)));
}
void VideoCaptureDeviceTest::RunGetPhotoStateTestCase() {
  const auto descriptor = FindUsableDeviceDescriptor();
  ASSERT_TRUE(descriptor);

  const gfx::Size frame_size = GetSupportedCaptureSize(*descriptor);
  if (frame_size == gfx::Size(0, 0))
    return;

#if defined(OS_ANDROID)
  // TODO(mcasas): fails on Lollipop devices, reconnect https://crbug.com/646840
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_MARSHMALLOW) {
    return;
  }
#endif

  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(*descriptor));
  ASSERT_TRUE(device);

  EXPECT_CALL(*video_capture_client_, OnError(_, _, _)).Times(0);
  EXPECT_CALL(*video_capture_client_, OnStarted());

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size = frame_size;
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));

  VideoCaptureDevice::GetPhotoStateCallback scoped_get_callback =
      base::BindOnce(&MockImageCaptureClient::DoOnGetPhotoState,
                     image_capture_client_);

  // On Chrome OS AllocateAndStart() is asynchronous, so wait until we get the
  // first frame.
  WaitForCapturedFrame();
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::Closure quit_closure = BindToCurrentLoop(run_loop.QuitClosure());
  EXPECT_CALL(*image_capture_client_.get(), OnCorrectGetPhotoState())
      .Times(1)
      .WillOnce(RunClosure(quit_closure));

  device->GetPhotoState(std::move(scoped_get_callback));
  run_loop.Run();

  ASSERT_TRUE(image_capture_client_->capabilities());

  device->StopAndDeAllocate();
}

#if defined(OS_WIN)
// Verifies that the photo callback is correctly released by MediaFoundation
WRAPPED_TEST_P(VideoCaptureDeviceTest,
               MAYBE_UsingRealWebcam_CheckPhotoCallbackRelease) {
  if (!UseWinMediaFoundation())
    return;

  std::unique_ptr<VideoCaptureDeviceDescriptor> descriptor =
      GetFirstDeviceDescriptorSupportingPixelFormat(PIXEL_FORMAT_MJPEG);
  ASSERT_TRUE(descriptor);

  EXPECT_CALL(*video_capture_client_, OnError(_, _, _)).Times(0);
  EXPECT_CALL(*video_capture_client_, OnStarted());

  std::unique_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->CreateDevice(*descriptor));
  ASSERT_TRUE(device);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(320, 240);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_MJPEG;
  device->AllocateAndStart(capture_params, std::move(video_capture_client_));

  if (!static_cast<VideoCaptureDeviceMFWin*>(device.get())
           ->get_use_photo_stream_to_take_photo_for_testing()) {
    DVLOG(1) << "The device is not using the MediaFoundation photo callback. "
                "Exiting test.";
    device->StopAndDeAllocate();
    return;
  }

  MockMFPhotoCallback* callback = new MockMFPhotoCallback();
  EXPECT_CALL(*callback, DoQueryInterface(_, _)).WillRepeatedly(Return(S_OK));
  EXPECT_CALL(*callback, DoAddRef()).WillOnce(Return(1U));
  EXPECT_CALL(*callback, DoRelease()).WillOnce(Return(1U));
  EXPECT_CALL(*callback, DoOnSample(_)).WillOnce(Return(S_OK));
  static_cast<VideoCaptureDeviceMFWin*>(device.get())
      ->set_create_mf_photo_callback_for_testing(base::BindRepeating(
          &VideoCaptureDeviceTest::CreateMockPhotoCallback,
          base::Unretained(this), base::Unretained(callback)));

  VideoCaptureDevice::TakePhotoCallback scoped_callback = base::BindOnce(
      &MockImageCaptureClient::DoOnPhotoTaken, image_capture_client_);

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::RepeatingClosure quit_closure =
      BindToCurrentLoop(run_loop.QuitClosure());
  EXPECT_CALL(*image_capture_client_.get(), OnCorrectPhotoTaken())
      .WillOnce(RunClosure(quit_closure));

  device->TakePhoto(std::move(scoped_callback));
  run_loop.Run();

  device->StopAndDeAllocate();
}
#endif

}  // namespace media
