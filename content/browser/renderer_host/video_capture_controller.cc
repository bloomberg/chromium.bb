// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/video_capture_controller.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "content/browser/browser_thread.h"
#include "content/browser/media_stream/video_capture_manager.h"
#include "media/base/yuv_convert.h"

#if defined(OS_WIN)
#include "content/common/section_util_win.h"
#endif

// The number of TransportDIBs VideoCaptureController allocate.
static const size_t kNoOfDIBS = 3;

VideoCaptureController::VideoCaptureController(
    ControllerId id,
    base::ProcessHandle render_process,
    EventHandler* event_handler)
    : render_handle_(render_process),
      report_ready_to_delete_(false),
      event_handler_(event_handler),
      id_(id) {}

VideoCaptureController::~VideoCaptureController() {
  // Delete all TransportDIBs.
  STLDeleteContainerPairSecondPointers(owned_dibs_.begin(),
                                       owned_dibs_.end());
}

void VideoCaptureController::StartCapture(
    const media::VideoCaptureParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  params_ = params;
  media_stream::VideoCaptureManager* manager =
      media_stream::VideoCaptureManager::Get();
  // Order the manager to start the actual capture.
  manager->Start(params, this);
}

void VideoCaptureController::StopCapture(Task* stopped_task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  media_stream::VideoCaptureManager* manager =
      media_stream::VideoCaptureManager::Get();
  manager->Stop(params_.session_id,
                NewRunnableMethod(this,
                                  &VideoCaptureController::OnDeviceStopped,
                                  stopped_task));
}

void VideoCaptureController::ReturnTransportDIB(TransportDIB::Handle handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool ready_to_delete;
  {
    base::AutoLock lock(lock_);
    free_dibs_.push_back(handle);
    ready_to_delete = (free_dibs_.size() == owned_dibs_.size());
  }
  if (report_ready_to_delete_ && ready_to_delete) {
    event_handler_->OnReadyToDelete(id_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Implements VideoCaptureDevice::EventHandler.
// OnIncomingCapturedFrame is called the thread running the capture device.
// I.e.- DirectShow thread on windows and v4l2_thread on Linux.
void VideoCaptureController::OnIncomingCapturedFrame(const uint8* data,
                                                     int length,
                                                     base::Time timestamp) {
  TransportDIB::Handle handle;
  TransportDIB* dib = NULL;
  // Check if there is a TransportDIB to fill.
  bool buffer_exist = false;
  {
    base::AutoLock lock(lock_);
    if (!report_ready_to_delete_ && free_dibs_.size() > 0) {
      handle = free_dibs_.back();
      free_dibs_.pop_back();
      DIBMap::iterator it = owned_dibs_.find(handle);
      if (it != owned_dibs_.end()) {
        dib = it->second;
        buffer_exist = true;
      }
    }
  }

  if (!buffer_exist) {
    return;
  }

  if (!dib->Map()) {
    VLOG(1) << "OnIncomingCapturedFrame - Failed to map handle.";
    base::AutoLock lock(lock_);
    free_dibs_.push_back(handle);
    return;
  }
  uint8* target = static_cast<uint8*>(dib->memory());
  CHECK(dib->size() >= static_cast<size_t> (frame_info_.width *
                                            frame_info_.height * 3) /
                                            2);

  // Do color conversion from the camera format to I420.
  switch (frame_info_.color) {
    case media::VideoCaptureDevice::kColorUnknown:  // Color format not set.
      break;
    case media::VideoCaptureDevice::kI420: {
      memcpy(target, data, (frame_info_.width * frame_info_.height * 3) / 2);
      break;
    }
    case media::VideoCaptureDevice::kYUY2: {
      uint8* yplane = target;
      uint8* uplane = target + frame_info_.width * frame_info_.height;
      uint8* vplane = uplane + (frame_info_.width * frame_info_.height) / 4;
      media::ConvertYUY2ToYUV(data, yplane, uplane, vplane, frame_info_.width,
                              frame_info_.height);
      break;
    }
    case media::VideoCaptureDevice::kRGB24: {
#if defined(OS_WIN)  // RGB on Windows start at the bottom line.
      uint8* yplane = target;
      uint8* uplane = target + frame_info_.width * frame_info_.height;
      uint8* vplane = uplane + (frame_info_.width * frame_info_.height) / 4;
      int ystride = frame_info_.width;
      int uvstride = frame_info_.width / 2;
      int rgb_stride = - 3 * frame_info_.width;
      const uint8* rgb_src = data + 3 * frame_info_.width *
          (frame_info_.height -1);
#else
      uint8* yplane = target;
      uint8* uplane = target + frame_info_.width * frame_info_.height;
      uint8* vplane = uplane + (frame_info_.width * frame_info_.height) / 4;
      int ystride = frame_info_.width;
      int uvstride = frame_info_.width / 2;
      int rgb_stride = 3 * frame_info_.width;
      const uint8* rgb_src = data;
#endif
      media::ConvertRGB24ToYUV(rgb_src, yplane, uplane, vplane,
                               frame_info_.width, frame_info_.height,
                               rgb_stride, ystride, uvstride);
      break;
    }
    case media::VideoCaptureDevice::kARGB: {
      uint8* yplane = target;
      uint8* uplane = target +  frame_info_.width * frame_info_.height;
      uint8* vplane = uplane +  (frame_info_.width * frame_info_.height) / 4;
      media::ConvertRGB32ToYUV(data, yplane, uplane, vplane, frame_info_.width,
                               frame_info_.height, frame_info_.width * 4,
                               frame_info_.width, frame_info_.width / 2);
      break;
    }
    default:
      NOTREACHED();
  }

  event_handler_->OnBufferReady(id_, handle, timestamp);
}

void VideoCaptureController::OnError() {
  event_handler_->OnError(id_);
}

void VideoCaptureController::OnFrameInfo(
    const media::VideoCaptureDevice::Capability& info) {
  DCHECK(owned_dibs_.empty());
  bool frames_created = true;
  const size_t needed_size = (info.width * info.height * 3) / 2;
  for (size_t i = 0; i < kNoOfDIBS; ++i) {
    TransportDIB* dib = TransportDIB::Create(needed_size, i);
    if (!dib) {
      frames_created = false;
      break;
    }
    // Lock needed since the buffers are used in OnIncomingFrame
    // and we need to use it there in order to avoid memcpy of complete frames.
    base::AutoLock lock(lock_);
#if defined(OS_WIN)
    // On Windows we need to get a handle the can be used in the render process.
    TransportDIB::Handle handle = chrome::GetSectionForProcess(
        dib->handle(), render_handle_, false);
#else
    TransportDIB::Handle handle = dib->handle();
#endif
    owned_dibs_.insert(std::make_pair(handle, dib));
    free_dibs_.push_back(handle);
  }
  frame_info_= info;

  // Check that all DIBs where created successfully.
  if (!frames_created) {
    event_handler_->OnError(id_);
  }
  event_handler_->OnFrameInfo(id_, info.width, info.height,
                              info.frame_rate);
}

///////////////////////////////////////////////////////////////////////////////
// Called by VideoCaptureManager when a device have been stopped.
// This will report to the event handler that this object is ready to be deleted
// if all DIBS have been returned.
void VideoCaptureController::OnDeviceStopped(Task* stopped_task) {
  bool ready_to_delete_now;

  // Set flag to indicate we need to report when all DIBs have been returned.
  report_ready_to_delete_ = true;
  {
    base::AutoLock lock(lock_);
    ready_to_delete_now = (free_dibs_.size() == owned_dibs_.size());
  }

  if (ready_to_delete_now) {
    event_handler_->OnReadyToDelete(id_);
  }
  if (stopped_task) {
    stopped_task->Run();
    delete stopped_task;
  }
}
