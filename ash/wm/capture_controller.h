// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CAPTURE_CONTROLLER_H_
#define ASH_WM_CAPTURE_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/capture_client.h"
#include "ash/ash_export.h"

namespace ash {
namespace internal {

class ASH_EXPORT CaptureController : public aura::client::CaptureClient {
 public:
  CaptureController();
  virtual ~CaptureController();

  // Overridden from aura::client::CaptureClient:
  virtual void SetCapture(aura::Window* window) OVERRIDE;
  virtual void ReleaseCapture(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetCaptureWindow() OVERRIDE;

 private:
  // The current capture window. Null if there is no capture window.
  aura::Window* capture_window_;

  DISALLOW_COPY_AND_ASSIGN(CaptureController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_CAPTURE_CONTROLLER_H_
