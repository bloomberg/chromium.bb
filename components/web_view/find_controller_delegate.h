// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_FIND_CONTROLLER_DELEGATE_H_
#define COMPONENTS_WEB_VIEW_FIND_CONTROLLER_DELEGATE_H_

#include <deque>

namespace mojom {
class WebViewClient;
}

namespace web_view {

class Frame;

class FindControllerDelegate {
 public:
  ~FindControllerDelegate() {}

  virtual std::deque<Frame*> GetAllFrames() = 0;

  virtual mojom::WebViewClient* GetWebViewClient() = 0;
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_FIND_CONTROLLER_DELEGATE_H_
