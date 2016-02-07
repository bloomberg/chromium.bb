// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_FRAME_DELEGATE_H_
#define COMPONENTS_HTML_VIEWER_HTML_FRAME_DELEGATE_H_

namespace mojo {
class Shell;
}

namespace html_viewer {

class HTMLFactory;
class HTMLFrame;

class HTMLFrameDelegate {
 public:
  // Returns the Shell for the frame.
  virtual mojo::Shell* GetShell() = 0;

  // Returns the factory for creating various classes.
  virtual HTMLFactory* GetHTMLFactory() = 0;

  // Invoked when the Frame the delegate is attached to finishes loading. This
  // is not invoked for any child frames, only the frame returned from
  // HTMLFrameTreeManager::CreateFrameAndAttachToTree().
  virtual void OnFrameDidFinishLoad() = 0;

  // Invoked when the HTMLFrame the delegate is associated with is swapped
  // to a remote frame.
  virtual void OnFrameSwappedToRemote() = 0;

  // Invoked when this delegate is to take over from |old_delegate| (which may
  // be null). This is invoked when a new connection is established and we
  // are already rendering to the supplied frame.
  virtual void OnSwap(HTMLFrame* frame, HTMLFrameDelegate* old_delegate) = 0;

  // Invoked when the HTMLFrame is destroyed.
  virtual void OnFrameDestroyed() = 0;

 protected:
  virtual ~HTMLFrameDelegate() {}
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_FRAME_DELEGATE_H_
