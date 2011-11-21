// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_TRACKING_H_
#define CHROME_RENDERER_RENDERER_TRACKING_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/process.h"
#include "base/task.h"
#include "content/public/renderer/render_process_observer.h"

class RendererTracking : public content::RenderProcessObserver {
 public:
  RendererTracking();
  virtual ~RendererTracking();

 private:
  // RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnGetRendererTrackingData(int sequence_number);

  void OnSetTrackingStatus(bool enable);

  // Extract tracking data from TrackedObjects and then send it off the
  // Browser process.
  void UploadAllTrackingData(int sequence_number);

  ScopedRunnableMethodFactory<RendererTracking> renderer_tracking_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererTracking);
};

#endif  // CHROME_RENDERER_RENDERER_TRACKING_H_
