// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BROWSER_PLUGIN_CHROME_BROWSER_PLUGIN_DELEGATE_H_
#define CHROME_RENDERER_BROWSER_PLUGIN_CHROME_BROWSER_PLUGIN_DELEGATE_H_

#include "content/public/renderer/browser_plugin_delegate.h"

#include "content/public/renderer/render_frame_observer.h"
#include "ipc/ipc_listener.h"

// TODO(lazyboy): This should live under /extensions.
class ChromeBrowserPluginDelegate : public content::BrowserPluginDelegate,
                                    public content::RenderFrameObserver {
 public:
  ChromeBrowserPluginDelegate(content::RenderFrame* render_frame,
                              const std::string& mime_type);
  virtual ~ChromeBrowserPluginDelegate();

  // BrowserPluginDelegate implementation.
  virtual void SetElementInstanceID(int element_instance_id) OVERRIDE;
  virtual void DidFinishLoading() OVERRIDE;
  virtual void DidReceiveData(const char* data, int data_length) OVERRIDE;

  // RenderFrameObserver override.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnCreateMimeHandlerViewGuestACK(int browser_plugin_delegate);

  const std::string mime_type_;
  int element_instance_id_;
  std::string html_string_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserPluginDelegate);
};

#endif  // CHROME_RENDERER_BROWSER_PLUGIN_CHROME_BROWSER_PLUGIN_DELEGATE_H_
