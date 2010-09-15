// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_POLICY_SETTINGS_H_
#define CHROME_FRAME_POLICY_SETTINGS_H_

#include <string>
#include <vector>

// A simple class that reads and caches policy settings for Chrome Frame.
// TODO(tommi): Support refreshing when new settings are pushed.
// TODO(tommi): Use Chrome's classes for this (and the notification service).
class PolicySettings {
 public:
  typedef enum RendererForUrl {
    RENDERER_NOT_SPECIFIED = -1,
    RENDER_IN_HOST,
    RENDER_IN_CHROME_FRAME,
  };

  PolicySettings() : default_renderer_(RENDERER_NOT_SPECIFIED) {
    RefreshFromRegistry();
  }

  ~PolicySettings() {
  }

  RendererForUrl default_renderer() const {
    return default_renderer_;
  }

  RendererForUrl GetRendererForUrl(const wchar_t* url);

 protected:
  // Protected for now since the class is not thread safe.
  void RefreshFromRegistry();

 protected:
  RendererForUrl default_renderer_;
  std::vector<std::wstring> renderer_exclusion_list_;
};


#endif  // CHROME_FRAME_POLICY_SETTINGS_H_
