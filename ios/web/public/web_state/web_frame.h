// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_FRAME_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_FRAME_H_

#include <string>

#include "base/macros.h"
#include "url/gurl.h"

namespace web {

class WebFrame {
 public:
  // The frame identifier which uniquely identifies this frame across the
  // application's lifetime.
  virtual std::string GetFrameId() const = 0;
  // Whether or not the receiver represents the main frame of the webpage.
  virtual bool IsMainFrame() const = 0;
  // The security origin associated with this frame.
  virtual GURL GetSecurityOrigin() const = 0;

  virtual ~WebFrame() {}

 protected:
  WebFrame() {}

  DISALLOW_COPY_AND_ASSIGN(WebFrame);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_FRAME_H_
