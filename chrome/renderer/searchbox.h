// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_H_
#define CHROME_RENDERER_SEARCHBOX_H_
#pragma once

#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/common/instant_types.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "ui/gfx/rect.h"

class SearchBox : public content::RenderViewObserver,
                  public content::RenderViewObserverTracker<SearchBox> {
 public:
  explicit SearchBox(content::RenderView* render_view);
  virtual ~SearchBox();

  // Sends ViewHostMsg_SetSuggestions to the browser.
  void SetSuggestions(const std::vector<std::string>& suggestions,
                      InstantCompleteBehavior behavior);

  const string16& value() const { return value_; }
  bool verbatim() const { return verbatim_; }
  uint32 selection_start() const { return selection_start_; }
  uint32 selection_end() const { return selection_end_; }
  gfx::Rect GetRect();

 private:
  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

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

  // Sets the searchbox values to their initial value.
  void Reset();

  string16 value_;
  bool verbatim_;
  uint32 selection_start_;
  uint32 selection_end_;
  gfx::Rect rect_;

  DISALLOW_COPY_AND_ASSIGN(SearchBox);
};

#endif  // CHROME_RENDERER_SEARCHBOX_H_
