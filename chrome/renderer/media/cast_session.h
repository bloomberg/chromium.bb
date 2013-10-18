// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_SESSION_H_
#define CHROME_RENDERER_MEDIA_CAST_SESSION_H_

#include "base/basictypes.h"

// This class represents a Cast session which consists of the
// following three major components:
// 1. Video and audio input.
// 2. Cast RTP transport.
// 3. Network connection.
//
// This class connects the above three components to provide a Cast
// service.
class CastSession {
 public:
  CastSession();
  ~CastSession();

 private:
  DISALLOW_COPY_AND_ASSIGN(CastSession);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_SESSION_H_
