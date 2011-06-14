// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_capture_message_filter_creator.h"

#include "content/renderer/media/video_capture_message_filter.h"

VideoCaptureMessageFilterCreator::VideoCaptureMessageFilterCreator() {
  filter_ = new VideoCaptureMessageFilter(1);
}

VideoCaptureMessageFilterCreator::~VideoCaptureMessageFilterCreator() {
}

// static
VideoCaptureMessageFilter* VideoCaptureMessageFilterCreator::SharedFilter() {
  return GetInstance()->filter_.get();
}

// static
VideoCaptureMessageFilterCreator*
    VideoCaptureMessageFilterCreator::GetInstance() {
      return Singleton<VideoCaptureMessageFilterCreator>::get();
}

