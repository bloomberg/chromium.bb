// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_HOST_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_HOST_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class ExtensionWindowController;
class FaviconTabHelper;
class GURL;
class Panel;
class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

// Helper class for Panel, implementing WebContents hosting and Extension
// delegates. Owned and used by Panel only.
class PanelHost : public content::WebContentsDelegate,
                  public content::WebContentsObserver,
                  public ExtensionFunctionDispatcher::Delegate {
 public:
  PanelHost(Panel* panel, Profile* profile);
  virtual ~PanelHost();

  void Init(const GURL& url);
  content::WebContents* web_contents() { return web_contents_.get(); }
  void DestroyWebContents();

  // Returns the icon for the current page.
  SkBitmap GetPageIcon() const;

  // content::WebContentsDelegate overrides.
  virtual void NavigationStateChanged(const content::WebContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual void ActivateContents(content::WebContents* contents) OVERRIDE;
  virtual void DeactivateContents(content::WebContents* contents) OVERRIDE;
  virtual void LoadingStateChanged(content::WebContents* source) OVERRIDE;
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual void MoveContents(content::WebContents* source,
                            const gfx::Rect& pos) OVERRIDE;
  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE;
  virtual bool IsApplication() const OVERRIDE;
  virtual void ResizeDueToAutoResize(content::WebContents* web_contents,
                                     const gfx::Size& new_size) OVERRIDE;

  // content::WebContentsObserver overrides.
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate overrides.
  virtual ExtensionWindowController* GetExtensionWindowController() const
      OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

 private:
  // Helper to close panel via the message loop.
  void ClosePanel();

  // Message handlers.
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  Panel* panel_;  // Weak, owns us.
  Profile* profile_;
  ExtensionFunctionDispatcher extension_function_dispatcher_;

  // The following factory is used to close the panel via the message loop.
  base::WeakPtrFactory<PanelHost> weak_factory_;

  scoped_ptr<FaviconTabHelper> favicon_tab_helper_;
  scoped_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PanelHost);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_HOST_H_
