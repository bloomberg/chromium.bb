// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_IDLE_USER_DETECTOR_H_
#define CONTENT_RENDERER_IDLE_USER_DETECTOR_H_

#include "base/basictypes.h"
#include "content/public/renderer/render_view_observer.h"

// Class which observes user input events and postpones
// idle notifications if the user is active.
class IdleUserDetector : public content::RenderViewObserver {
 public:
  IdleUserDetector(content::RenderView* render_view);
  virtual ~IdleUserDetector();

 private:
  // RenderViewObserver implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnHandleInputEvent(const IPC::Message& message);

  DISALLOW_COPY_AND_ASSIGN(IdleUserDetector);
};

#endif  // CONTENT_RENDERER_IDLE_USER_DETECTOR_H_
