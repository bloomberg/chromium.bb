// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_EXTERNAL_RENDERER_H_
#define CONTENT_RENDERER_MEDIA_EXTERNAL_RENDERER_H_

// TODO(ronghuawu) ExternalRenderer should be replaced by
// cricket::VideoRenderer from libjingle
class ExternalRenderer {
 public:
  virtual int FrameSizeChange(unsigned int width,
                              unsigned int height,
                              unsigned int number_of_streams) = 0;
  virtual int DeliverFrame(unsigned char* buffer, int buffer_size) = 0;

 protected:
  virtual ~ExternalRenderer() {}
};

#endif  // CONTENT_RENDERER_MEDIA_EXTERNAL_RENDERER_H_
