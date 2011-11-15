// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PROFILER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PROFILER_UI_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"

// The C++ back-end for the chrome://profiler webui page.
class ProfilerUI : public ChromeWebUI {
 public:
  explicit ProfilerUI(TabContents* contents);
  virtual ~ProfilerUI();

  // Get the tracking data from TrackingSynchronizer.
  void GetData();

  // Send the data to the renderer.
  void ReceivedData(base::Value* value);

 private:
  // Used to get |weak_ptr_| to self on the UI thread.
  scoped_ptr<base::WeakPtrFactory<ProfilerUI> > ui_weak_ptr_factory_;
  base::WeakPtr<ProfilerUI> ui_weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ProfilerUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PROFILER_UI_H_
