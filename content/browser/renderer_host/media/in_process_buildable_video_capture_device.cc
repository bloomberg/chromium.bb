// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/in_process_buildable_video_capture_device.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_gpu_jpeg_decoder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/media_stream_request.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"

#if defined(ENABLE_SCREEN_CAPTURE) && !defined(OS_ANDROID)
#include "content/browser/media/capture/desktop_capture_device.h"
#if defined(USE_AURA)
#include "content/browser/media/capture/desktop_capture_device_aura.h"
#endif
#endif

#if defined(ENABLE_SCREEN_CAPTURE) && defined(OS_ANDROID)
#include "content/browser/media/capture/screen_capture_device_android.h"
#endif

namespace {

class VideoFrameConsumerFeedbackObserverOnTaskRunner
    : public media::VideoFrameConsumerFeedbackObserver {
 public:
  VideoFrameConsumerFeedbackObserverOnTaskRunner(
      media::VideoFrameConsumerFeedbackObserver* observer,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : observer_(observer), task_runner_(std::move(task_runner)) {}

  void OnUtilizationReport(int frame_feedback_id, double utilization) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &media::VideoFrameConsumerFeedbackObserver::OnUtilizationReport,
            base::Unretained(observer_), frame_feedback_id, utilization));
  }

 private:
  media::VideoFrameConsumerFeedbackObserver* const observer_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

std::unique_ptr<media::VideoCaptureJpegDecoder> CreateGpuJpegDecoder(
    const media::VideoCaptureJpegDecoder::DecodeDoneCB& decode_done_cb) {
  return base::MakeUnique<content::VideoCaptureGpuJpegDecoder>(decode_done_cb);
}

void StopAndReleaseDeviceOnDeviceThread(media::VideoCaptureDevice* device,
                                        base::OnceClosure done_cb) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StopDeviceTime");
  device->StopAndDeAllocate();
  DVLOG(3) << "StopAndReleaseDeviceOnDeviceThread";
  delete device;
  base::ResetAndReturn(&done_cb).Run();
}

// The maximum number of video frame buffers in-flight at any one time. This
// value should be based on the logical capacity of the capture pipeline, and
// not on hardware performance.  For example, tab capture requires more buffers
// than webcam capture because the pipeline is longer (it includes read-backs
// pending in the GPU pipeline).
const int kMaxNumberOfBuffers = 3;
// TODO(miu): The value for tab capture should be determined programmatically.
// http://crbug.com/460318
const int kMaxNumberOfBuffersForTabCapture = 10;

}  // anonymous namespace

namespace content {

InProcessBuildableVideoCaptureDevice::InProcessBuildableVideoCaptureDevice(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
    media::VideoCaptureSystem* video_capture_system)
    : device_task_runner_(std::move(device_task_runner)),
      video_capture_system_(video_capture_system) {}

InProcessBuildableVideoCaptureDevice::~InProcessBuildableVideoCaptureDevice() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!device_);
}

void InProcessBuildableVideoCaptureDevice::CreateAndStartDeviceAsync(
    VideoCaptureController* controller,
    const media::VideoCaptureParams& params,
    Callbacks* callbacks,
    base::OnceClosure done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(State::NO_DEVICE, state_);

  const int max_buffers = (controller->stream_type() == MEDIA_TAB_VIDEO_CAPTURE
                               ? kMaxNumberOfBuffersForTabCapture
                               : kMaxNumberOfBuffers);

  auto device_client =
      CreateDeviceClient(max_buffers, controller->GetWeakPtrForIOThread());

  base::Closure start_capture_closure;
  // Use of Unretained() is safe, because |done_cb| guarantees that
  // |this| stays alive.
  ReceiveDeviceCallback after_start_capture_callback = media::BindToCurrentLoop(
      base::Bind(&InProcessBuildableVideoCaptureDevice::OnDeviceStarted,
                 base::Unretained(this), controller, callbacks,
                 base::Passed(&done_cb)));

  switch (controller->stream_type()) {
    case MEDIA_DEVICE_VIDEO_CAPTURE: {
      start_capture_closure =
          base::Bind(&InProcessBuildableVideoCaptureDevice::
                         DoStartDeviceCaptureOnDeviceThread,
                     base::Unretained(this), controller->device_id(), params,
                     base::Passed(std::move(device_client)),
                     std::move(after_start_capture_callback));
      break;
    }
    case MEDIA_TAB_VIDEO_CAPTURE:
      start_capture_closure =
          base::Bind(&InProcessBuildableVideoCaptureDevice::
                         DoStartTabCaptureOnDeviceThread,
                     base::Unretained(this), controller->device_id(), params,
                     base::Passed(std::move(device_client)),
                     std::move(after_start_capture_callback));
      break;

    case MEDIA_DESKTOP_VIDEO_CAPTURE:
      start_capture_closure =
          base::Bind(&InProcessBuildableVideoCaptureDevice::
                         DoStartDesktopCaptureOnDeviceThread,
                     base::Unretained(this), controller->device_id(), params,
                     base::Passed(std::move(device_client)),
                     std::move(after_start_capture_callback));
      break;

    default: {
      NOTIMPLEMENTED();
      return;
    }
  }

  device_task_runner_->PostTask(FROM_HERE, start_capture_closure);
  state_ = State::DEVICE_START_IN_PROGRESS;
}

void InProcessBuildableVideoCaptureDevice::ReleaseDeviceAsync(
    VideoCaptureController* controller,
    base::OnceClosure done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  controller->SetConsumerFeedbackObserver(nullptr);
  switch (state_) {
    case State::DEVICE_START_IN_PROGRESS:
      state_ = State::DEVICE_START_ABORTING;
      return;
    case State::NO_DEVICE:
    case State::DEVICE_START_ABORTING:
      return;
    case State::DEVICE_STARTED:
      media::VideoCaptureDevice* device_ptr = device_.release();
      bool posting_task_succeeded = device_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &StopAndReleaseDeviceOnDeviceThread, device_ptr,
              base::Bind([](scoped_refptr<base::SingleThreadTaskRunner>) {},
                         device_task_runner_)));
      if (posting_task_succeeded == false) {
        // Since posting to the task runner has failed, we attempt doing it on
        // the calling thread instead.
        StopAndReleaseDeviceOnDeviceThread(device_ptr, base::Bind([]() {}));
      }
      state_ = State::NO_DEVICE;
      return;
  }
  base::ResetAndReturn(&done_cb).Run();
}

bool InProcessBuildableVideoCaptureDevice::IsDeviceAlive() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return device_ != nullptr;
}

void InProcessBuildableVideoCaptureDevice::GetPhotoCapabilities(
    media::VideoCaptureDevice::GetPhotoCapabilitiesCallback callback) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Unretained() is safe to use here because |device| would be null if it
  // was scheduled for shutdown and destruction, and because this task is
  // guaranteed to run before the task that destroys the |device|.
  device_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&media::VideoCaptureDevice::GetPhotoCapabilities,
                 base::Unretained(device_.get()), base::Passed(&callback)));
}

void InProcessBuildableVideoCaptureDevice::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    media::VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Unretained() is safe to use here because |device| would be null if it
  // was scheduled for shutdown and destruction, and because this task is
  // guaranteed to run before the task that destroys the |device|.
  device_task_runner_->PostTask(
      FROM_HERE, base::Bind(&media::VideoCaptureDevice::SetPhotoOptions,
                            base::Unretained(device_.get()),
                            base::Passed(&settings), base::Passed(&callback)));
}

void InProcessBuildableVideoCaptureDevice::TakePhoto(
    media::VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Unretained() is safe to use here because |device| would be null if it
  // was scheduled for shutdown and destruction, and because this task is
  // guaranteed to run before the task that destroys the |device|.
  device_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&media::VideoCaptureDevice::TakePhoto,
                 base::Unretained(device_.get()), base::Passed(&callback)));
}

void InProcessBuildableVideoCaptureDevice::MaybeSuspendDevice() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Unretained() is safe to use here because |device| would be null if it
  // was scheduled for shutdown and destruction, and because this task is
  // guaranteed to run before the task that destroys the |device|.
  device_task_runner_->PostTask(
      FROM_HERE, base::Bind(&media::VideoCaptureDevice::MaybeSuspend,
                            base::Unretained(device_.get())));
}

void InProcessBuildableVideoCaptureDevice::ResumeDevice() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Unretained() is safe to use here because |device| would be null if it
  // was scheduled for shutdown and destruction, and because this task is
  // guaranteed to run before the task that destroys the |device|.
  device_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&media::VideoCaptureDevice::Resume,
                                           base::Unretained(device_.get())));
}

void InProcessBuildableVideoCaptureDevice::RequestRefreshFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Unretained() is safe to use here because |device| would be null if it
  // was scheduled for shutdown and destruction, and because this task is
  // guaranteed to run before the task that destroys the |device|.
  device_task_runner_->PostTask(
      FROM_HERE, base::Bind(&media::VideoCaptureDevice::RequestRefreshFrame,
                            base::Unretained(device_.get())));
}

void InProcessBuildableVideoCaptureDevice::SetDesktopCaptureWindowIdAsync(
    gfx::NativeViewId window_id,
    base::OnceClosure done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Post |device_| to the the |device_task_runner_|. This is safe since the
  // device is destroyed on the |device_task_runner_| and |done_cb| guarantees
  // that |this| stays alive.
  device_task_runner_->PostTask(
      FROM_HERE, base::Bind(&InProcessBuildableVideoCaptureDevice::
                                SetDesktopCaptureWindowIdOnDeviceThread,
                            base::Unretained(this), device_.get(), window_id,
                            base::Passed(&done_cb)));
}

std::unique_ptr<media::VideoCaptureDeviceClient>
InProcessBuildableVideoCaptureDevice::CreateDeviceClient(
    int buffer_pool_max_buffer_count,
    base::WeakPtr<media::VideoFrameReceiver> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return base::MakeUnique<media::VideoCaptureDeviceClient>(
      base::MakeUnique<media::VideoFrameReceiverOnTaskRunner>(
          receiver, BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)),
      new media::VideoCaptureBufferPoolImpl(
          base::MakeUnique<media::VideoCaptureBufferTrackerFactoryImpl>(),
          buffer_pool_max_buffer_count),
      base::Bind(&CreateGpuJpegDecoder,
                 base::Bind(&media::VideoFrameReceiver::OnFrameReadyInBuffer,
                            receiver)));
}

void InProcessBuildableVideoCaptureDevice::OnDeviceStarted(
    VideoCaptureController* controller,
    Callbacks* callbacks,
    base::OnceClosure done_cb,
    std::unique_ptr<media::VideoCaptureDevice> device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (state_) {
    case State::DEVICE_START_IN_PROGRESS:
      if (!device) {
        state_ = State::NO_DEVICE;
        callbacks->OnDeviceStartFailed(controller);
        base::ResetAndReturn(&done_cb).Run();
        return;
      }
      // Passing raw pointer |device.get()| to the controller is safe,
      // because we take ownership of |device| and we call
      // controller->SetConsumerFeedbackObserver(nullptr) before releasing
      // |device|.
      controller->SetConsumerFeedbackObserver(
          base::MakeUnique<VideoFrameConsumerFeedbackObserverOnTaskRunner>(
              device.get(), device_task_runner_));
      device_ = std::move(device);
      state_ = State::DEVICE_STARTED;
      callbacks->OnDeviceStarted(controller);
      base::ResetAndReturn(&done_cb).Run();
      return;
    case State::DEVICE_START_ABORTING:
      if (device) {
        device_ = std::move(device);
        state_ = State::DEVICE_STARTED;
        // We do not move our |done_cb| to this invocation, because
        // we still need it to stay alive for the remainder of this method
        // execution. Our implementation of ReleaseDeviceAsync() does not
        // actually need the context while releasing the device.
        ReleaseDeviceAsync(controller, base::Bind([]() {}));
      }
      state_ = State::NO_DEVICE;
      callbacks->OnDeviceStartAborted();
      base::ResetAndReturn(&done_cb).Run();
      return;
    case State::NO_DEVICE:
    case State::DEVICE_STARTED:
      NOTREACHED();
      return;
  }
}

void InProcessBuildableVideoCaptureDevice::DoStartDeviceCaptureOnDeviceThread(
    const std::string& device_id,
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDeviceClient> device_client,
    ReceiveDeviceCallback result_callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(device_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<media::VideoCaptureDevice> video_capture_device =
      video_capture_system_->CreateDevice(device_id);

  if (!video_capture_device) {
    result_callback.Run(nullptr);
    return;
  }

  video_capture_device->AllocateAndStart(params, std::move(device_client));
  result_callback.Run(std::move(video_capture_device));
}

void InProcessBuildableVideoCaptureDevice::DoStartTabCaptureOnDeviceThread(
    const std::string& id,
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDeviceClient> device_client,
    ReceiveDeviceCallback result_callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(device_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<media::VideoCaptureDevice> video_capture_device;
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
  video_capture_device = WebContentsVideoCaptureDevice::Create(id);
#endif

  if (!video_capture_device) {
    result_callback.Run(nullptr);
    return;
  }

  video_capture_device->AllocateAndStart(params, std::move(device_client));
  result_callback.Run(std::move(video_capture_device));
}

void InProcessBuildableVideoCaptureDevice::DoStartDesktopCaptureOnDeviceThread(
    const std::string& id,
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDeviceClient> device_client,
    ReceiveDeviceCallback result_callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(device_task_runner_->BelongsToCurrentThread());

  std::unique_ptr<media::VideoCaptureDevice> video_capture_device;
#if defined(ENABLE_SCREEN_CAPTURE)
  DesktopMediaID desktop_id = DesktopMediaID::Parse(id);
  if (desktop_id.is_null()) {
    DLOG(ERROR) << "Desktop media ID is null";
    result_callback.Run(nullptr);
    return;
  }

  if (desktop_id.type == DesktopMediaID::TYPE_WEB_CONTENTS) {
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
    video_capture_device = WebContentsVideoCaptureDevice::Create(id);
    IncrementDesktopCaptureCounter(TAB_VIDEO_CAPTURER_CREATED);
    if (desktop_id.audio_share)
      IncrementDesktopCaptureCounter(TAB_VIDEO_CAPTURER_CREATED_WITH_AUDIO);
    else
      IncrementDesktopCaptureCounter(TAB_VIDEO_CAPTURER_CREATED_WITHOUT_AUDIO);
#endif
  } else {
#if defined(OS_ANDROID)
    video_capture_device = base::MakeUnique<ScreenCaptureDeviceAndroid>();
#else
#if defined(USE_AURA)
    video_capture_device = DesktopCaptureDeviceAura::Create(desktop_id);
#endif  // defined(USE_AURA)
#if BUILDFLAG(ENABLE_WEBRTC)
    if (!video_capture_device)
      video_capture_device = DesktopCaptureDevice::Create(desktop_id);
#endif  // BUILDFLAG(ENABLE_WEBRTC)
#endif  // defined (OS_ANDROID)
  }
#endif  // defined(ENABLE_SCREEN_CAPTURE)

  if (!video_capture_device) {
    result_callback.Run(nullptr);
    return;
  }

  video_capture_device->AllocateAndStart(params, std::move(device_client));
  result_callback.Run(std::move(video_capture_device));
}

void InProcessBuildableVideoCaptureDevice::
    SetDesktopCaptureWindowIdOnDeviceThread(media::VideoCaptureDevice* device,
                                            gfx::NativeViewId window_id,
                                            base::OnceClosure done_cb) {
  DCHECK(device_task_runner_->BelongsToCurrentThread());
#if defined(ENABLE_SCREEN_CAPTURE) && BUILDFLAG(ENABLE_WEBRTC) && \
    !defined(OS_ANDROID)
  DesktopCaptureDevice* desktop_device =
      static_cast<DesktopCaptureDevice*>(device);
  desktop_device->SetNotificationWindowId(window_id);
  VLOG(2) << "Screen capture notification window passed on device thread.";
#endif
  base::ResetAndReturn(&done_cb).Run();
}

}  // namespace content
