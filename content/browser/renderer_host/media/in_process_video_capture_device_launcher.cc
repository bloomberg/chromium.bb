// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/in_process_video_capture_device_launcher.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/renderer_host/media/in_process_launched_video_capture_device.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_gpu_jpeg_decoder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/media_stream_request.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_frame_receiver.h"
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

std::unique_ptr<media::VideoCaptureJpegDecoder> CreateGpuJpegDecoder(
    media::VideoCaptureJpegDecoder::DecodeDoneCB decode_done_cb,
    base::Callback<void(const std::string&)> send_log_message_cb) {
  return base::MakeUnique<content::VideoCaptureGpuJpegDecoder>(
      std::move(decode_done_cb), std::move(send_log_message_cb));
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

InProcessVideoCaptureDeviceLauncher::InProcessVideoCaptureDeviceLauncher(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
    media::VideoCaptureSystem* video_capture_system)
    : device_task_runner_(std::move(device_task_runner)),
      video_capture_system_(video_capture_system),
      state_(State::READY_TO_LAUNCH) {}

InProcessVideoCaptureDeviceLauncher::~InProcessVideoCaptureDeviceLauncher() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(state_ == State::READY_TO_LAUNCH);
}

void InProcessVideoCaptureDeviceLauncher::LaunchDeviceAsync(
    const std::string& device_id,
    MediaStreamType stream_type,
    const media::VideoCaptureParams& params,
    base::WeakPtr<media::VideoFrameReceiver> receiver,
    base::OnceClosure /* connection_lost_cb */,
    Callbacks* callbacks,
    base::OnceClosure done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(state_ == State::READY_TO_LAUNCH);
  if (receiver) {
    std::ostringstream string_stream;
    string_stream
        << "InProcessVideoCaptureDeviceLauncher::LaunchDeviceAsync: Posting "
           "start request to device thread for device_id = "
        << device_id;
    receiver->OnLog(string_stream.str());
  }

  const int max_buffers =
      (stream_type == MEDIA_TAB_VIDEO_CAPTURE ? kMaxNumberOfBuffersForTabCapture
                                              : kMaxNumberOfBuffers);

  auto device_client = CreateDeviceClient(max_buffers, std::move(receiver));

  base::Closure start_capture_closure;
  // Use of Unretained |this| is safe, because |done_cb| guarantees that |this|
  // stays alive.
  ReceiveDeviceCallback after_start_capture_callback = media::BindToCurrentLoop(
      base::Bind(&InProcessVideoCaptureDeviceLauncher::OnDeviceStarted,
                 base::Unretained(this), callbacks, base::Passed(&done_cb)));

  switch (stream_type) {
    case MEDIA_DEVICE_VIDEO_CAPTURE: {
      if (!video_capture_system_) {
        // Clients who create an instance of |this| without providing a
        // VideoCaptureSystem instance are expected to know that
        // MEDIA_DEVICE_VIDEO_CAPTURE is not supported in this case.
        NOTREACHED();
        return;
      }
      start_capture_closure =
          base::Bind(&InProcessVideoCaptureDeviceLauncher::
                         DoStartDeviceCaptureOnDeviceThread,
                     base::Unretained(this), device_id, params,
                     base::Passed(std::move(device_client)),
                     std::move(after_start_capture_callback));
      break;
    }
    case MEDIA_TAB_VIDEO_CAPTURE:
      start_capture_closure = base::Bind(
          &InProcessVideoCaptureDeviceLauncher::DoStartTabCaptureOnDeviceThread,
          base::Unretained(this), device_id, params,
          base::Passed(std::move(device_client)),
          std::move(after_start_capture_callback));
      break;

    case MEDIA_DESKTOP_VIDEO_CAPTURE:
      start_capture_closure =
          base::Bind(&InProcessVideoCaptureDeviceLauncher::
                         DoStartDesktopCaptureOnDeviceThread,
                     base::Unretained(this), device_id, params,
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

void InProcessVideoCaptureDeviceLauncher::AbortLaunch() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (state_ == State::DEVICE_START_IN_PROGRESS)
    state_ = State::DEVICE_START_ABORTING;
}

std::unique_ptr<media::VideoCaptureDeviceClient>
InProcessVideoCaptureDeviceLauncher::CreateDeviceClient(
    int buffer_pool_max_buffer_count,
    base::WeakPtr<media::VideoFrameReceiver> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool =
      new media::VideoCaptureBufferPoolImpl(
          base::MakeUnique<media::VideoCaptureBufferTrackerFactoryImpl>(),
          buffer_pool_max_buffer_count);

  return base::MakeUnique<media::VideoCaptureDeviceClient>(
      base::MakeUnique<media::VideoFrameReceiverOnTaskRunner>(
          receiver, BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)),
      std::move(buffer_pool),
      base::Bind(&CreateGpuJpegDecoder,
                 base::Bind(&media::VideoFrameReceiver::OnFrameReadyInBuffer,
                            receiver),
                 base::Bind(&media::VideoFrameReceiver::OnLog, receiver)));
}

void InProcessVideoCaptureDeviceLauncher::OnDeviceStarted(
    Callbacks* callbacks,
    base::OnceClosure done_cb,
    std::unique_ptr<media::VideoCaptureDevice> device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  State state_copy = state_;
  state_ = State::READY_TO_LAUNCH;
  if (!device) {
    switch (state_copy) {
      case State::DEVICE_START_IN_PROGRESS:
        callbacks->OnDeviceLaunchFailed();
        base::ResetAndReturn(&done_cb).Run();
        return;
      case State::DEVICE_START_ABORTING:
        callbacks->OnDeviceLaunchAborted();
        base::ResetAndReturn(&done_cb).Run();
        return;
      case State::READY_TO_LAUNCH:
        NOTREACHED();
        return;
    }
  }

  auto launched_device = base::MakeUnique<InProcessLaunchedVideoCaptureDevice>(
      std::move(device), device_task_runner_);

  switch (state_copy) {
    case State::DEVICE_START_IN_PROGRESS:
      callbacks->OnDeviceLaunched(std::move(launched_device));
      base::ResetAndReturn(&done_cb).Run();
      return;
    case State::DEVICE_START_ABORTING:
      launched_device.reset();
      callbacks->OnDeviceLaunchAborted();
      base::ResetAndReturn(&done_cb).Run();
      return;
    case State::READY_TO_LAUNCH:
      NOTREACHED();
      return;
  }
}

void InProcessVideoCaptureDeviceLauncher::DoStartDeviceCaptureOnDeviceThread(
    const std::string& device_id,
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDeviceClient> device_client,
    ReceiveDeviceCallback result_callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(device_task_runner_->BelongsToCurrentThread());
  DCHECK(video_capture_system_);

  std::unique_ptr<media::VideoCaptureDevice> video_capture_device =
      video_capture_system_->CreateDevice(device_id);

  if (!video_capture_device) {
    result_callback.Run(nullptr);
    return;
  }

  video_capture_device->AllocateAndStart(params, std::move(device_client));
  result_callback.Run(std::move(video_capture_device));
}

void InProcessVideoCaptureDeviceLauncher::DoStartTabCaptureOnDeviceThread(
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

void InProcessVideoCaptureDeviceLauncher::DoStartDesktopCaptureOnDeviceThread(
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

  if (desktop_id.source_type == DesktopMediaID::SOURCE_WEB_CONTENTS) {
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
    video_capture_device = WebContentsVideoCaptureDevice::Create(id);
    IncrementDesktopCaptureCounter(TAB_VIDEO_CAPTURER_CREATED);
    if (desktop_id.is_audio_capture()) {
      IncrementDesktopCaptureCounter(TAB_VIDEO_CAPTURER_CREATED_WITH_AUDIO);
    } else {
      IncrementDesktopCaptureCounter(TAB_VIDEO_CAPTURER_CREATED_WITHOUT_AUDIO);
    }
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

}  // namespace content
