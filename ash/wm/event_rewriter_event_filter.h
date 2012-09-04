// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_EVENT_REWRITER_EVENT_FILTER_
#define ASH_WM_EVENT_REWRITER_EVENT_FILTER_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/event_filter.h"

namespace ash {

class EventRewriterDelegate;

namespace internal {

// An event filter that rewrites or drops an event.
class ASH_EXPORT EventRewriterEventFilter : public aura::EventFilter {
 public:
  EventRewriterEventFilter();
  virtual ~EventRewriterEventFilter();

  void SetEventRewriterDelegate(scoped_ptr<EventRewriterDelegate> delegate);

 private:
  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 ui::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      aura::Window* target,
      ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult PreHandleGestureEvent(
      aura::Window* target,
      ui::GestureEvent* event) OVERRIDE;

  scoped_ptr<EventRewriterDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriterEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_EVENT_REWRITER_EVENT_FILTER_
