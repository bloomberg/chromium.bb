// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEY_REWRITER_DELEGATE_H_
#define ASH_KEY_REWRITER_DELEGATE_H_
#pragma once

namespace aura {
class KeyEvent;
}  // namespace aura

namespace ash {

// Delegate for rewriting or filtering a key event.
class KeyRewriterDelegate {
 public:
  enum Action {
    ACTION_REWRITE_EVENT,
    ACTION_DROP_EVENT,
  };

  virtual ~KeyRewriterDelegate() {}

  // A derived class can do either of the following:
  // 1) Just return ACTION_DROP_EVENT to drop the |event|.
  // 2) Rewrite the |event| and return ACTION_REWRITE_EVENT.
  virtual Action RewriteOrFilterKeyEvent(aura::KeyEvent* event) = 0;
};

}  // namespace ash

#endif  // ASH_KEY_REWRITER_DELEGATE_H_
