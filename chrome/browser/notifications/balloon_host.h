// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class Balloon;
class Browser;

namespace content {
class SiteInstance;
};

class BalloonHost : public content::WebContentsDelegate,
                    public content::WebContentsObserver,
                    public ExtensionFunctionDispatcher::Delegate {
 public:
  explicit BalloonHost(Balloon* balloon);

  // Initialize the view.
  void Init();

  // Stops showing the balloon.
  void Shutdown();

  // ExtensionFunctionDispatcher::Delegate overrides.
  virtual ExtensionWindowController* GetExtensionWindowController()
      const OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  const string16& GetSource() const;

  content::WebContents* web_contents() const { return web_contents_.get(); }

  // Enable Web UI. This has to be called before renderer is created.
  void EnableWebUI();

  // Returns whether the associated render view is ready. Used only for testing.
  bool IsRenderViewReady() const;

  // content::WebContentsDelegate implementation:
  virtual bool CanLoadDataURLsInWebUI() const OVERRIDE;

 protected:
  virtual ~BalloonHost();

  scoped_ptr<content::WebContents> web_contents_;

 private:
  // content::WebContentsDelegate implementation:
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual void HandleMouseDown() OVERRIDE;
  virtual void ResizeDueToAutoResize(content::WebContents* source,
                                     const gfx::Size& pref_size) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE;

  // content::WebContentsObserver implementation:
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
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
  scoped_refptr<content::SiteInstance> site_instance_;

  // A flag to enable Web UI.
  bool enable_web_ui_;

  ExtensionFunctionDispatcher extension_function_dispatcher_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_HOST_H_
