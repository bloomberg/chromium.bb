// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_BALLOON_HOST_H_
#define CHROME_BROWSER_NOTIFICATIONS_BALLOON_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/common/notification_registrar.h"

class Balloon;
class Browser;
class Profile;
class SiteInstance;
struct RendererPreferences;
struct WebPreferences;

class BalloonHost : public RenderViewHostDelegate,
                    public RenderViewHostDelegate::View,
                    public ExtensionFunctionDispatcher::Delegate,
                    public NotificationObserver {
 public:
  explicit BalloonHost(Balloon* balloon);

  // Initialize the view.
  void Init();

  // Stops showing the balloon.
  void Shutdown();

  // ExtensionFunctionDispatcher::Delegate overrides.
  virtual Browser* GetBrowser();
  virtual gfx::NativeView GetNativeViewOfHost();
  virtual TabContents* associated_tab_contents() const;

  RenderViewHost* render_view_host() const { return render_view_host_; }

  const string16& GetSource() const;

  // RenderViewHostDelegate overrides.
  virtual WebPreferences GetWebkitPrefs();
  virtual SiteInstance* GetSiteInstance() const;
  virtual Profile* GetProfile() const;
  virtual const GURL& GetURL() const;
  virtual void Close(RenderViewHost* render_view_host);
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void RenderViewReady(RenderViewHost* render_view_host);
  virtual void RenderViewGone(RenderViewHost* render_view_host,
                              base::TerminationStatus status,
                              int error_code);
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id, const std::wstring& title) {}
  virtual int GetBrowserWindowID() const;
  virtual ViewType::Type GetRenderViewType() const;
  virtual RenderViewHostDelegate::View* GetViewDelegate();
  virtual void ProcessDOMUIMessage(const ViewHostMsg_DomMessage_Params& params);

  // NotificationObserver override.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);


  // RenderViewHostDelegate::View methods. Only the ones for opening new
  // windows are currently implemented.
  virtual void CreateNewWindow(
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params);
  virtual void CreateNewWidget(int route_id, WebKit::WebPopupType popup_type) {}
  virtual void CreateNewFullscreenWidget(
      int route_id, WebKit::WebPopupType popup_type) {}
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture);
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) {}
  virtual void ShowCreatedFullscreenWidget(int route_id) {}
  virtual void ShowContextMenu(const ContextMenuParams& params) {}
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned) {}
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_ops) {}
  virtual void StartDragging(const WebDropData&,
                             WebKit::WebDragOperationsMask,
                             const SkBitmap&,
                             const gfx::Point&) {}
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) {}
  virtual void GotFocus() {}
  virtual void TakeFocus(bool reverse) {}
  virtual void LostCapture() {}
  virtual void Activate() {}
  virtual void Deactivate() {}
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}
  virtual void HandleMouseMove() {}
  virtual void HandleMouseDown();
  virtual void HandleMouseLeave() {}
  virtual void HandleMouseUp() {}
  virtual void HandleMouseActivate() {}
  virtual void UpdatePreferredSize(const gfx::Size& pref_size);
  virtual RendererPreferences GetRendererPrefs(Profile* profile) const;

  // Enable Web UI. This has to be called before renderer is created.
  void EnableDOMUI();

  virtual void UpdateInspectorSetting(const std::string& key,
                                      const std::string& value);
  virtual void ClearInspectorSettings();

 protected:
  virtual ~BalloonHost();
  // Must override in platform specific implementations.
  virtual void InitRenderWidgetHostView() = 0;
  virtual RenderWidgetHostView* render_widget_host_view() const = 0;

  // Owned pointer to the host for the renderer process.
  RenderViewHost* render_view_host_;

 private:
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

  // Handles requests to extension APIs. Will only be non-NULL if we are
  // rendering a page from an extension.
  scoped_ptr<ExtensionFunctionDispatcher> extension_function_dispatcher_;

  // A flag to enable Web UI.
  bool enable_dom_ui_;

  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_BALLOON_HOST_H_
