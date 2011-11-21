// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOON_HOST_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOON_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_registrar.h"

class Balloon;
class Browser;
class SiteInstance;

namespace IPC {
class Message;
}

class BalloonHost : public TabContentsDelegate,
                    public TabContentsObserver,
                    public ExtensionFunctionDispatcher::Delegate {
 public:
  explicit BalloonHost(Balloon* balloon);

  // Initialize the view.
  void Init();

  // Stops showing the balloon.
  void Shutdown();

  // ExtensionFunctionDispatcher::Delegate overrides.
  virtual Browser* GetBrowser() OVERRIDE;
  virtual gfx::NativeView GetNativeViewOfHost() OVERRIDE;
  virtual TabContents* GetAssociatedTabContents() const OVERRIDE;

  const string16& GetSource() const;

  TabContents* tab_contents() const { return tab_contents_.get(); }

  // Enable Web UI. This has to be called before renderer is created.
  void EnableWebUI();

  // Returns whether the associated render view is ready. Used only for testing.
  bool IsRenderViewReady() const;

 protected:
  virtual ~BalloonHost();

  scoped_ptr<TabContents> tab_contents_;

 private:
  // TabContentsDelegate implementation:
  virtual void CloseContents(TabContents* source) OVERRIDE;
  virtual void HandleMouseDown() OVERRIDE;
  virtual void UpdatePreferredSize(TabContents* source,
                                   const gfx::Size& pref_size) OVERRIDE;

  // TabContentsObserver implementation:
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewReady() OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Message handlers
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  // Called to send an event that the balloon has been disconnected from
  // a renderer (if should_notify_on_disconnect_ is true).
  void NotifyDisconnect();

  // Non-owned pointer to the associated balloon.
  Balloon* balloon_;

  // True after Init() has completed.
  bool initialized_;

  // Indicates whether we should notify about disconnection of this balloon.
  // This is used to ensure disconnection notifications only happen if
  // a connection notification has happened and that they happen only once.
  bool should_notify_on_disconnect_;

  // Site instance for the balloon/profile, to be used for opening new links.
  scoped_refptr<SiteInstance> site_instance_;

  // A flag to enable Web UI.
  bool enable_web_ui_;

  ExtensionFunctionDispatcher extension_function_dispatcher_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_HOST_H_
