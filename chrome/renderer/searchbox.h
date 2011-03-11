// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_H_
#define CHROME_RENDERER_SEARCHBOX_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "chrome/renderer/render_view_observer.h"
#include "chrome/common/instant_types.h"
#include "ui/gfx/rect.h"

class SearchBox : public RenderViewObserver {
 public:
  explicit SearchBox(RenderView* render_view);
  ~SearchBox();

  // Sends ViewHostMsg_SetSuggestions to the browser.
  void SetSuggestions(const std::vector<std::string>& suggestions,
                      InstantCompleteBehavior behavior);

  const string16& value() { return value_; }
  bool verbatim() { return verbatim_; }
  uint32 selection_start() { return selection_start_; }
  uint32 selection_end() { return selection_end_; }
  const gfx::Rect& rect() { return rect_; }

 private:
  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  void OnChange(const string16& value,
                bool verbatim,
                int selection_start,
                int selection_end);
  void OnSubmit(const string16& value, bool verbatim);
  void OnCancel();
  void OnResize(const gfx::Rect& bounds);
  void OnDetermineIfPageSupportsInstant(const string16& value,
                                        bool verbatim,
                                        int selection_start,
                                        int selection_end);

  string16 value_;
  bool verbatim_;
  uint32 selection_start_;
  uint32 selection_end_;
  gfx::Rect rect_;

  DISALLOW_COPY_AND_ASSIGN(SearchBox);
};

#endif  // CHROME_RENDERER_SEARCHBOX_H_
