// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_CREATOR_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_CREATOR_H_

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"

class VideoCaptureMessageFilter;

// VideoCaptureMessageFilterCreator is to be used as a singleton so we can get
// access to a shared VideoCaptureMessageFilter.
// Example usage:
//   VideoCaptureMessageFilter* filter =
//       VideoCaptureMessageFilterCreator::SharedFilter();

class VideoCaptureMessageFilterCreator {
 public:
  static VideoCaptureMessageFilter* SharedFilter();
  static VideoCaptureMessageFilterCreator* GetInstance();

 private:
  VideoCaptureMessageFilterCreator();
  ~VideoCaptureMessageFilterCreator();
  friend struct DefaultSingletonTraits<VideoCaptureMessageFilterCreator>;

  scoped_refptr<VideoCaptureMessageFilter> filter_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureMessageFilterCreator);
};

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_CREATOR_H_

