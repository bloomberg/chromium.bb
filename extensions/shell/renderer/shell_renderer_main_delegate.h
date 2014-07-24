// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_RENDERER_SHELL_RENDERER_MAIN_DELEGATE_H_
#define EXTENSIONS_SHELL_RENDERER_SHELL_RENDERER_MAIN_DELEGATE_H_

namespace content {
class RenderThread;
class RenderView;
}

namespace extensions {

class ShellRendererMainDelegate {
 public:
  virtual ~ShellRendererMainDelegate() {}

  // Called when |thread| is started, after the extensions subsystem has been
  // initialized for |thread|.
  virtual void OnThreadStarted(content::RenderThread* thread) = 0;

  // Called for each RenderView created in the renderer process, after the
  // extension related code has been initialized for the view.
  virtual void OnViewCreated(content::RenderView* view) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_RENDERER_SHELL_RENDERER_MAIN_DELEGATE_H_
