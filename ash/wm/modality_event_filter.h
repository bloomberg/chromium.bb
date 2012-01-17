// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MODALITY_EVENT_FILTER_H_
#define ASH_WM_MODALITY_EVENT_FILTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/aura/event_filter.h"
#include "ash/ash_export.h"

namespace ash {
namespace internal {

class ModalityEventFilterDelegate;

class ASH_EXPORT ModalityEventFilter : public aura::EventFilter {
 public:
  ModalityEventFilter(aura::Window* container,
                      ModalityEventFilterDelegate* delegate);
  virtual ~ModalityEventFilter();

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      aura::Window* target,
      aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

 private:
  ModalityEventFilterDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ModalityEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_MODALITY_EVENT_FILTER_H_
