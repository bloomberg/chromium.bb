// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENT_REWRITER_DELEGATE_H_
#define ASH_EVENT_REWRITER_DELEGATE_H_

namespace ui {
class KeyEvent;
class LocatedEvent;
}  // namespace aura

namespace ash {

// Delegate for rewriting or filtering an event.
class EventRewriterDelegate {
 public:
  enum Action {
    ACTION_REWRITE_EVENT,
    ACTION_DROP_EVENT,
  };

  virtual ~EventRewriterDelegate() {}

  // A derived class can do either of the following:
  // 1) Just return ACTION_DROP_EVENT to drop the |event|.
  // 2) Rewrite the |event| and return ACTION_REWRITE_EVENT.
  virtual Action RewriteOrFilterKeyEvent(ui::KeyEvent* event) = 0;
  virtual Action RewriteOrFilterLocatedEvent(ui::LocatedEvent* event) = 0;
};

}  // namespace ash

#endif  // ASH_EVENT_REWRITER_DELEGATE_H_
