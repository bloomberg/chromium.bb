// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TURNDOWN_PROMPT_RESHOW_STATE_H_
#define CHROME_FRAME_TURNDOWN_PROMPT_RESHOW_STATE_H_

#include "base/time/time.h"

namespace turndown_prompt {

class ReshowState {
 public:
  explicit ReshowState(const base::TimeDelta& reshow_delta);
  ~ReshowState();

  bool HasReshowDeltaExpired(const base::Time& current_time) const;
  void MarkShown(const base::Time& last_shown_time);

 private:
  base::TimeDelta reshow_delta_;
};

}  // namespace turndown_prompt

#endif  // CHROME_FRAME_TURNDOWN_PROMPT_RESHOW_STATE_H_
