// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_CONTENTS_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_CONTENTS_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/view_types.h"
#include "chrome/common/window_container_type.h"
#include "webkit/glue/window_open_disposition.h"

class TabContents;
class PrerenderManager;
struct WebPreferences;
struct ViewHostMsg_FrameNavigate_Params;

namespace gfx {
class Rect;
}

// This class is a peer of TabContents. It can host a renderer, but does not
// have any visible display. Its navigation is not managed by a
// NavigationController because is has no facility for navigating (other than
// programatically view window.location.href) or RenderViewHostManager because
// it is never allowed to navigate across a SiteInstance boundary.
class PrerenderContents : public RenderViewHostDelegate,
                          public RenderViewHostDelegate::View,
                          public NotificationObserver,
                          public JavaScriptAppModalDialogDelegate {
 public:
  // PrerenderContents::Create uses the currently registered Factory to create
  // the PrerenderContents. Factory is intended for testing.
  class Factory {
   public:
    Factory() {}
    virtual ~Factory() {}

    virtual PrerenderContents* CreatePrerenderContents(
        PrerenderManager* prerender_manager, Profile* profile, const GURL& url,
        const std::vector<GURL>& alias_urls) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  virtual ~PrerenderContents();

  static Factory* CreateFactory();

  virtual void StartPrerendering();

  RenderViewHost* render_view_host() { return render_view_host_; }
  // Allows replacing of the RenderViewHost owned by this class, including
  // replacing with a NULL value.  When a caller uses this, the caller will
  // own (and is responsible for freeing) the old RVH.
  void set_render_view_host(RenderViewHost* rvh) { render_view_host_ = rvh; }
  ViewHostMsg_FrameNavigate_Params* navigate_params() {
    return navigate_params_.get();
  }
  string16 title() const { return title_; }
  int32 page_id() const { return page_id_; }
  bool has_stopped_loading() const { return has_stopped_loading_; }

  // Indicates whether this prerendered page can be used for the provided
  // URL, i.e. whether there is a match.
  bool MatchesURL(const GURL& url) const;

  // RenderViewHostDelegate implementation.
  virtual RenderViewHostDelegate::View* GetViewDelegate();
  virtual const GURL& GetURL() const;
  virtual ViewType::Type GetRenderViewType() const;
  virtual int GetBrowserWindowID() const;
  virtual void DidNavigate(RenderViewHost* render_view_host,
                           const ViewHostMsg_FrameNavigate_Params& params);
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::wstring& title);
  virtual WebPreferences GetWebkitPrefs();
  virtual void ProcessDOMUIMessage(const ViewHostMsg_DomMessage_Params& params);
  virtual void RunJavaScriptMessage(const std::wstring& message,
                                    const std::wstring& default_prompt,
                                    const GURL& frame_url,
                                    const int flags,
                                    IPC::Message* reply_msg,
                                    bool* did_suppress_message);
  virtual void Close(RenderViewHost* render_view_host);
  virtual void DidStopLoading();
  virtual RendererPreferences GetRendererPrefs(Profile* profile) const;

  // RenderViewHostDelegate::View
  virtual void CreateNewWindow(
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params);
  virtual void CreateNewWidget(int route_id, WebKit::WebPopupType popup_type);
  virtual void CreateNewFullscreenWidget(
      int route_id, WebKit::WebPopupType popup_type);
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture);
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos);
  virtual void ShowCreatedFullscreenWidget(int route_id);
  virtual void ShowContextMenu(const ContextMenuParams& params) {}
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned) {}
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_operations,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) {}
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
  virtual void HandleMouseDown() {}
  virtual void HandleMouseLeave() {}
  virtual void HandleMouseUp() {}
  virtual void HandleMouseActivate() {}
  virtual void UpdatePreferredSize(const gfx::Size& new_size) {}

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from JavaScriptAppModalDialogDelegate:
  virtual void OnMessageBoxClosed(IPC::Message* reply_msg,
                                  bool success,
                                  const std::wstring& prompt);
  virtual void SetSuppressMessageBoxes(bool suppress_message_boxes) {}
  virtual gfx::NativeWindow GetMessageBoxRootWindow();
  virtual TabContents* AsTabContents();
  virtual ExtensionHost* AsExtensionHost();

  virtual void UpdateInspectorSetting(const std::string& key,
                                      const std::string& value);
  virtual void ClearInspectorSettings();

 protected:
  PrerenderContents(PrerenderManager* prerender_manager, Profile* profile,
                    const GURL& url, const std::vector<GURL>& alias_urls);

  // from RenderViewHostDelegate.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  // Needs to be able to call the constructor.
  friend class PrerenderContentsFactoryImpl;

  // Message handlers.
  void OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                         bool main_frame,
                                         const GURL& url);
  void OnDidRedirectProvisionalLoad(int32 page_id,
                                    const GURL& source_url,
                                    const GURL& target_url);

  void AddAliasURL(const GURL& url);

  // The prerender manager owning this object.
  PrerenderManager* prerender_manager_;

  // The host for our HTML content.
  RenderViewHost* render_view_host_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;

  // The URL being prerendered.
  GURL prerender_url_;

  // The NavigationParameters of the finished navigation.
  scoped_ptr<ViewHostMsg_FrameNavigate_Params> navigate_params_;

  // The profile being used
  Profile* profile_;

  // Information about the title and URL of the page that this class as a
  // RenderViewHostDelegate has received from the RenderView.
  // Used to apply to the new RenderViewHost delegate that might eventually
  // own the contained RenderViewHost when the prerendered page is shown
  // in a TabContents.
  string16 title_;
  int32 page_id_;
  GURL url_;
  NotificationRegistrar registrar_;

  // A vector of URLs that this prerendered page matches against.
  // This array can contain more than element as a result of redirects,
  // such as HTTP redirects or javascript redirects.
  std::vector<GURL> alias_urls_;

  bool has_stopped_loading_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderContents);
};

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_CONTENTS_H_
