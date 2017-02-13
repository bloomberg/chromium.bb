// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_WINDOW_PARAMS_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_WINDOW_PARAMS_H_

#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/common/referrer.h"
#include "third_party/WebKit/public/web/window_features.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

class BlockedWindowParams {
 public:
  BlockedWindowParams(const GURL& target_url,
                      const content::Referrer& referrer,
                      const std::string& frame_name_,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& features,
                      bool user_gesture,
                      bool opener_suppressed,
                      int render_process_id,
                      int opener_render_frame_id);
  BlockedWindowParams(const BlockedWindowParams& other);
  ~BlockedWindowParams();

  chrome::NavigateParams CreateNavigateParams(
      content::WebContents* web_contents) const;

  blink::mojom::WindowFeatures features() const { return features_; }

  int opener_render_frame_id() const {
    return opener_render_frame_id_;
  }

  int render_process_id() const {
    return render_process_id_;
  }

  const GURL& target_url() const {
    return target_url_;
  }

 private:
  GURL target_url_;
  content::Referrer referrer_;
  std::string frame_name_;
  WindowOpenDisposition disposition_;
  blink::mojom::WindowFeatures features_;
  bool user_gesture_;
  bool opener_suppressed_;
  int render_process_id_;
  int opener_render_frame_id_;
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_BLOCKED_WINDOW_PARAMS_H_
