// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_GUEST_DELEGATE_H_
#define CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_GUEST_DELEGATE_H_

#include "chrome/browser/extensions/api/web_view/chrome_web_view_internal_api.h"
#include "chrome/browser/ui/zoom/zoom_observer.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_guest_delegate.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#endif

class RenderViewContextMenu;

namespace ui {
class SimpleMenuModel;
}  // namespace ui

class ChromeWebViewGuestDelegate : public extensions::WebViewGuestDelegate,
                                   public ZoomObserver {
 public :
  explicit ChromeWebViewGuestDelegate(
      extensions::WebViewGuest* web_view_guest);
  virtual ~ChromeWebViewGuestDelegate();

  // WebViewGuestDelegate implementation.
  virtual double GetZoom() OVERRIDE;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;
  virtual void OnAttachWebViewHelpers(content::WebContents* contents) OVERRIDE;
  virtual void OnEmbedderDestroyed() OVERRIDE;
  virtual void OnDidAttachToEmbedder() OVERRIDE;
  virtual void OnDidCommitProvisionalLoadForFrame(bool is_main_frame) OVERRIDE;
  virtual void OnDidInitialize() OVERRIDE;
  virtual void OnDocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void OnGuestDestroyed() OVERRIDE;
  virtual void OnSetZoom(double zoom_factor) OVERRIDE;
  virtual void OnShowContextMenu(
      int request_id,
      const MenuItemVector* items) OVERRIDE;

  // ZoomObserver implementation.
  virtual void OnZoomChanged(
      const ZoomController::ZoomChangedEventData& data) OVERRIDE;

  extensions::WebViewGuest* web_view_guest() const { return web_view_guest_; }

 private:
  content::WebContents* guest_web_contents() const {
    return web_view_guest()->web_contents();
  }

  // Returns the top level items (ignoring submenus) as Value.
  static scoped_ptr<base::ListValue> MenuModelToValue(
      const ui::SimpleMenuModel& menu_model);

  void InjectChromeVoxIfNeeded(content::RenderViewHost* render_view_host);

#if defined(OS_CHROMEOS)
  // Notification of a change in the state of an accessibility setting.
  void OnAccessibilityStatusChanged(
      const chromeos::AccessibilityStatusEventDetails& details);
#endif

  // A counter to generate a unique request id for a context menu request.
  // We only need the ids to be unique for a given WebViewGuest.
  int pending_context_menu_request_id_;

  // Set to |true| if ChromeVox was already injected in main frame.
  bool chromevox_injected_;

  // Stores the current zoom factor.
  double current_zoom_factor_;

  // Holds the RenderViewContextMenu that has been built but yet to be
  // shown. This is .Reset() after ShowContextMenu().
  scoped_ptr<RenderViewContextMenu> pending_menu_;

#if defined(OS_CHROMEOS)
  // Subscription to receive notifications on changes to a11y settings.
  scoped_ptr<chromeos::AccessibilityStatusSubscription>
      accessibility_subscription_;
#endif

  extensions::WebViewGuest* const web_view_guest_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebViewGuestDelegate);
};

#endif  // CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_GUEST_DELEGATE_H_

