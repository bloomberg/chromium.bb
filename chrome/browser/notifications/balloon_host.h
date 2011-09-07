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
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class Balloon;
class Browser;
class Profile;
class SiteInstance;
struct RendererPreferences;
struct WebPreferences;

namespace IPC {
class Message;
}

class BalloonHost : public RenderViewHostDelegate,
                    public RenderViewHostDelegate::View,
                    public ExtensionFunctionDispatcher::Delegate {
 public:
  explicit BalloonHost(Balloon* balloon);

  // Initialize the view.
  void Init();

  // Stops showing the balloon.
  void Shutdown();

  // ExtensionFunctionDispatcher::Delegate overrides.
  virtual Browser* GetBrowser();
  virtual gfx::NativeView GetNativeViewOfHost();
  virtual TabContents* GetAssociatedTabContents() const;

  RenderViewHost* render_view_host() const { return render_view_host_; }

  const string16& GetSource() const;

  // RenderViewHostDelegate overrides.
  virtual WebPreferences GetWebkitPrefs() OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual void Close(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewReady(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewGone(RenderViewHost* render_view_host,
                              base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual ViewType::Type GetRenderViewType() const OVERRIDE;
  virtual RenderViewHostDelegate::View* GetViewDelegate() OVERRIDE;
  virtual void HandleMouseDown() OVERRIDE;
  virtual RendererPreferences GetRendererPrefs(
      content::BrowserContext* browser_context) const OVERRIDE;
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) OVERRIDE;

  // RenderViewHostDelegate::View methods. Only the ones for opening new
  // windows are currently implemented.
  virtual void CreateNewWindow(
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params) OVERRIDE;
  virtual void CreateNewWidget(int route_id,
                               WebKit::WebPopupType popup_type) OVERRIDE {}
  virtual void CreateNewFullscreenWidget(int route_id) OVERRIDE {}
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) OVERRIDE;
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) OVERRIDE {}
  virtual void ShowCreatedFullscreenWidget(int route_id) OVERRIDE {}
  virtual void ShowContextMenu(const ContextMenuParams& params) OVERRIDE {}
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned) OVERRIDE {}
  virtual void StartDragging(const WebDropData&,
                             WebKit::WebDragOperationsMask,
                             const SkBitmap&,
                             const gfx::Point&) OVERRIDE {}
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE {}
  virtual void GotFocus() OVERRIDE {}
  virtual void TakeFocus(bool reverse) OVERRIDE {}

  // Enable Web UI. This has to be called before renderer is created.
  void EnableWebUI();

  // Returns whether the associated render view is ready. Used only for testing.
  bool IsRenderViewReady() const;

 protected:
  virtual ~BalloonHost();
  // Must override in platform specific implementations.
  virtual void InitRenderWidgetHostView() = 0;
  virtual RenderWidgetHostView* render_widget_host_view() const = 0;

  // Owned pointer to the host for the renderer process.
  RenderViewHost* render_view_host_;

 private:
  // RenderViewHostDelegate
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

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;

  // A flag to enable Web UI.
  bool enable_web_ui_;

  ExtensionFunctionDispatcher extension_function_dispatcher_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_HOST_H_
