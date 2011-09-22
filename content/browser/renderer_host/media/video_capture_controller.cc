// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller.h"

#include "base/stl_util.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "media/base/yuv_convert.h"

// The number of TransportDIBs VideoCaptureController allocate.
static const size_t kNoOfDIBS = 3;

VideoCaptureController::VideoCaptureController(
    const VideoCaptureControllerID& id,
    base::ProcessHandle render_process,
    VideoCaptureControllerEventHandler* event_handler,
    media_stream::VideoCaptureManager* video_capture_manager)
    : render_handle_(render_process),
      report_ready_to_delete_(false),
      event_handler_(event_handler),
      id_(id),
      video_capture_manager_(video_capture_manager) {
  memset(&params_, 0, sizeof(params_));
}

VideoCaptureController::~VideoCaptureController() {
  // Delete all TransportDIBs.
  STLDeleteContainerPairSecondPointers(owned_dibs_.begin(),
                                       owned_dibs_.end());
}

void VideoCaptureController::StartCapture(
    const media::VideoCaptureParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  params_ = params;
  // Order the manager to start the actual capture.
  video_capture_manager_->Start(params, this);
}

void VideoCaptureController::StopCapture(Task* stopped_task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  video_capture_manager_->Stop(
      params_.session_id,
      NewRunnableMethod(this,
                        &VideoCaptureController::OnDeviceStopped,
                        stopped_task));
}

void VideoCaptureController::ReturnBuffer(int buffer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool ready_to_delete;
  {
    base::AutoLock lock(lock_);
    free_dibs_.push_back(buffer_id);
    ready_to_delete = (free_dibs_.size() == owned_dibs_.size()) &&
        report_ready_to_delete_;
  }
  if (ready_to_delete) {
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
  int buffer_id = 0;
  base::SharedMemory* dib = NULL;
  // Check if there is a TransportDIB to fill.
  bool buffer_exist = false;
  {
    base::AutoLock lock(lock_);
    if (!report_ready_to_delete_ && free_dibs_.size() > 0) {
      buffer_id = free_dibs_.front();
      free_dibs_.pop_front();
      DIBMap::iterator it = owned_dibs_.find(buffer_id);
      if (it != owned_dibs_.end()) {
        dib = it->second;
        buffer_exist = true;
      }
    }
  }

  if (!buffer_exist) {
    return;
  }

  uint8* target = static_cast<uint8*>(dib->memory());
  CHECK(dib->created_size() >= static_cast<size_t> (frame_info_.width *
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

  event_handler_->OnBufferReady(id_, buffer_id, timestamp);
}

void VideoCaptureController::OnError() {
  event_handler_->OnError(id_);
  video_capture_manager_->Error(params_.session_id);
}

void VideoCaptureController::OnFrameInfo(
    const media::VideoCaptureDevice::Capability& info) {
  DCHECK(owned_dibs_.empty());
  bool frames_created = true;
  const size_t needed_size = (info.width * info.height * 3) / 2;
  for (size_t i = 1; i <= kNoOfDIBS; ++i) {
    base::SharedMemory* shared_memory = new base::SharedMemory();
    if (!shared_memory->CreateAndMapAnonymous(needed_size)) {
      frames_created = false;
      break;
    }
    base::SharedMemoryHandle remote_handle;
    shared_memory->ShareToProcess(render_handle_, &remote_handle);

    base::AutoLock lock(lock_);
    owned_dibs_.insert(std::make_pair(i, shared_memory));
    free_dibs_.push_back(i);
    event_handler_->OnBufferCreated(id_, remote_handle,
                                    static_cast<int>(needed_size),
                                    static_cast<int>(i));
  }
  frame_info_= info;

  // Check that all DIBs where created successfully.
  if (!frames_created) {
    event_handler_->OnError(id_);
  }
  event_handler_->OnFrameInfo(id_, info.width, info.height, info.frame_rate);
}

///////////////////////////////////////////////////////////////////////////////
// Called by VideoCaptureManager when a device have been stopped.
// This will report to the event handler that this object is ready to be deleted
// if all DIBS have been returned.
void VideoCaptureController::OnDeviceStopped(Task* stopped_task) {
  bool ready_to_delete_now;

  {
    base::AutoLock lock(lock_);
    // Set flag to indicate we need to report when all DIBs have been returned.
    report_ready_to_delete_ = true;
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
