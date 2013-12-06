// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_

namespace base {
class FilePath;
}

namespace content {

// An interface implemented by an object interested in knowing about the state
// of the RenderFrameHost.
class RenderFrameHostDelegate {
 public:
  // The given Pepper plugin is not responsive.
  virtual void PepperPluginHung(int plugin_child_id,
                                const base::FilePath& path,
                                bool is_hung) {}

 protected:
  virtual ~RenderFrameHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_DELEGATE_H_
