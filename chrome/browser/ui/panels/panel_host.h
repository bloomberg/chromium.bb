// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_HOST_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_HOST_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/extension_function_dispatcher.h"

class GURL;
class Panel;
class PrefsTabHelper;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class WindowController;
}

namespace gfx {
class Image;
class Rect;
}

// Helper class for Panel, implementing WebContents hosting and Extension
// delegates. Owned and used by Panel only.
class PanelHost : public content::WebContentsDelegate,
                  public content::WebContentsObserver,
                  public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  PanelHost(Panel* panel, Profile* profile);
  ~PanelHost() override;

  void Init(const GURL& url);
  content::WebContents* web_contents() { return web_contents_.get(); }
  void DestroyWebContents();

  // Returns the icon for the current page.
  gfx::Image GetPageIcon() const;

  // content::WebContentsDelegate overrides.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void AddNewContents(content::WebContents* source,
                      content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void ActivateContents(content::WebContents* contents) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void CloseContents(content::WebContents* source) override;
  void MoveContents(content::WebContents* source,
                    const gfx::Rect& pos) override;
  bool IsPopupOrPanel(const content::WebContents* source) const override;
  void ContentsZoomChange(bool zoom_in) override;
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override;

  // content::WebContentsObserver overrides.
  void RenderProcessGone(base::TerminationStatus status) override;
  void WebContentsDestroyed() override;

  // extensions::ExtensionFunctionDispatcher::Delegate overrides.
  extensions::WindowController* GetExtensionWindowController() const override;
  content::WebContents* GetAssociatedWebContents() const override;

  // Actions on web contents.
  void Reload();
  void ReloadIgnoringCache();
  void StopLoading();
  void Zoom(content::PageZoom zoom);

 private:
  // Helper to close panel via the message loop.
  void ClosePanel();

  Panel* panel_;  // Weak, owns us.
  Profile* profile_;

  scoped_ptr<content::WebContents> web_contents_;

  // The following factory is used to close the panel via the message loop.
  base::WeakPtrFactory<PanelHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PanelHost);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_HOST_H_
