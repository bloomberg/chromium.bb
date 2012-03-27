// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_KEY_REWRITER_EVENT_FILTER_
#define ASH_WM_KEY_REWRITER_EVENT_FILTER_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/event_filter.h"

namespace ash {

class KeyRewriterDelegate;

namespace internal {

// An event filter that rewrites or drops a key event.
class ASH_EXPORT KeyRewriterEventFilter : public aura::EventFilter {
 public:
  KeyRewriterEventFilter();
  virtual ~KeyRewriterEventFilter();

  void SetKeyRewriterDelegate(scoped_ptr<KeyRewriterDelegate> delegate);

 private:
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

  scoped_ptr<KeyRewriterDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(KeyRewriterEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_KEY_REWRITER_EVENT_FILTER_
