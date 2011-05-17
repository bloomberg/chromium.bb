// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_CONTENTS_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_CONTENTS_H_
#pragma once

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_render_view_host_observer.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/window_container_type.h"
#include "webkit/glue/window_open_disposition.h"

class RenderViewHost;
class TabContents;
class TabContentsWrapper;
struct FaviconURL;
struct ViewHostMsg_FrameNavigate_Params;
struct WebPreferences;

namespace base {
class ProcessMetrics;
}

namespace gfx {
class Rect;
}

namespace prerender {

class PrerenderManager;

// This class is a peer of TabContents. It can host a renderer, but does not
// have any visible display. Its navigation is not managed by a
// NavigationController because is has no facility for navigating (other than
// programatically view window.location.href) or RenderViewHostManager because
// it is never allowed to navigate across a SiteInstance boundary.
// TODO(dominich): Remove RenderViewHostDelegate inheritance when UseTabContents
// returns true by default.
class PrerenderContents : public RenderViewHostDelegate,
                          public RenderViewHostDelegate::View,
                          public NotificationObserver,
                          public TabContentsObserver,
                          public JavaScriptAppModalDialogDelegate {
 public:
  // PrerenderContents::Create uses the currently registered Factory to create
  // the PrerenderContents. Factory is intended for testing.
  class Factory {
   public:
    Factory() {}
    virtual ~Factory() {}

    virtual PrerenderContents* CreatePrerenderContents(
        PrerenderManager* prerender_manager,
        Profile* profile,
        const GURL& url,
        const GURL& referrer) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  virtual ~PrerenderContents();

  bool Init();

  static Factory* CreateFactory();

  // |source_render_view_host| is the RenderViewHost that initiated
  // prerendering.  It must be non-NULL and have its own view.  It is used
  // solely to determine the window bounds while prerendering.
  virtual void StartPrerendering(const RenderViewHost* source_render_view_host);
  virtual void StartPrerenderingOld(
      const RenderViewHost* source_render_view_host);

  // Verifies that the prerendering is not using too many resources, and kills
  // it if not.
  void DestroyWhenUsingTooManyResources();

  RenderViewHost* render_view_host_mutable();
  const RenderViewHost* render_view_host() const;

  // Allows replacing of the RenderViewHost owned by this class, including
  // replacing with a NULL value.  When a caller uses this, the caller will
  // own (and is responsible for freeing) the old RVH.
  void set_render_view_host(RenderViewHost* render_view_host) {
    render_view_host_ = render_view_host;
  }
  ViewHostMsg_FrameNavigate_Params* navigate_params() {
    return navigate_params_.get();
  }
  string16 title() const { return title_; }
  int32 page_id() const { return page_id_; }
  GURL icon_url() const { return icon_url_; }
  bool has_stopped_loading() const { return has_stopped_loading_; }
  bool prerendering_has_started() const { return prerendering_has_started_; }

  // Sets the parameter to the value of the associated RenderViewHost's child id
  // and returns a boolean indicating the validity of that id.
  virtual bool GetChildId(int* child_id) const;

  // Sets the parameter to the value of the associated RenderViewHost's route id
  // and returns a boolean indicating the validity of that id.
  virtual bool GetRouteId(int* route_id) const;

  // Set the final status for how the PrerenderContents was used. This
  // should only be called once, and should be called before the prerender
  // contents are destroyed.
  void set_final_status(FinalStatus final_status);
  FinalStatus final_status() const;

  base::TimeTicks load_start_time() const { return load_start_time_; }

  // Indicates whether this prerendered page can be used for the provided
  // URL, i.e. whether there is a match.
  bool MatchesURL(const GURL& url) const;

  void OnJSOutOfMemory();
  void OnRunJavaScriptMessage(const std::wstring& message,
                              const std::wstring& default_prompt,
                              const GURL& frame_url,
                              const int flags,
                              bool* did_suppress_message,
                              std::wstring* prompt_field);
  virtual void OnRenderViewGone(int status, int exit_code);

  // RenderViewHostDelegate implementation.
  // TODO(dominich): Remove when RenderViewHostDelegate is removed as a base
  // class.
  virtual RenderViewHostDelegate::View* GetViewDelegate() OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual ViewType::Type GetRenderViewType() const OVERRIDE;
  virtual int GetBrowserWindowID() const OVERRIDE;
  virtual void DidNavigate(
      RenderViewHost* render_view_host,
      const ViewHostMsg_FrameNavigate_Params& params) OVERRIDE;
  virtual void UpdateTitle(RenderViewHost* render_view_host,
                           int32 page_id,
                           const std::wstring& title);
  virtual WebPreferences GetWebkitPrefs() OVERRIDE;
  virtual void Close(RenderViewHost* render_view_host) OVERRIDE;
  virtual RendererPreferences GetRendererPrefs(Profile* profile) const OVERRIDE;

  // TabContentsObserver implementation.
  virtual void DidStopLoading() OVERRIDE;

  // RenderViewHostDelegate::View
  // TODO(dominich): Remove when no longer a delegate for the view.
  virtual void CreateNewWindow(
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params) OVERRIDE;
  virtual void CreateNewWidget(int route_id,
                               WebKit::WebPopupType popup_type) OVERRIDE;
  virtual void CreateNewFullscreenWidget(int route_id) OVERRIDE;
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) OVERRIDE;
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) OVERRIDE;
  virtual void ShowCreatedFullscreenWidget(int route_id) OVERRIDE;
  virtual void ShowContextMenu(const ContextMenuParams& params) OVERRIDE {}
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned) OVERRIDE {}
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask allowed_operations,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) OVERRIDE {}
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE {}
  virtual void GotFocus() OVERRIDE {}
  virtual void TakeFocus(bool reverse) OVERRIDE {}
  virtual void LostCapture() OVERRIDE {}
  virtual void Activate() OVERRIDE {}
  virtual void Deactivate() OVERRIDE {}
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE {}
  virtual void HandleMouseMove() OVERRIDE {}
  virtual void HandleMouseDown() OVERRIDE {}
  virtual void HandleMouseLeave() OVERRIDE {}
  virtual void HandleMouseUp() OVERRIDE {}
  virtual void HandleMouseActivate() OVERRIDE {}
  virtual void UpdatePreferredSize(const gfx::Size& new_size) OVERRIDE {}

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Overridden from JavaScriptAppModalDialogDelegate:
  virtual void OnMessageBoxClosed(IPC::Message* reply_msg,
                                  bool success,
                                  const std::wstring& prompt) OVERRIDE;
  virtual void SetSuppressMessageBoxes(bool suppress_message_boxes) OVERRIDE {}
  virtual gfx::NativeWindow GetMessageBoxRootWindow() OVERRIDE;
  virtual TabContents* AsTabContents() OVERRIDE;
  virtual ExtensionHost* AsExtensionHost() OVERRIDE;

  virtual void UpdateInspectorSetting(const std::string& key,
                                      const std::string& value) OVERRIDE;
  virtual void ClearInspectorSettings() OVERRIDE;

  virtual void RendererUnresponsive(RenderViewHost* render_view_host,
                                    bool is_during_unload) OVERRIDE;

  // Adds an alias URL, for one of the many redirections. If the URL can not
  // be prerendered - for example, it's an ftp URL - |this| will be destroyed
  // and false is returned. Otherwise, true is returned and the alias is
  // remembered.
  bool AddAliasURL(const GURL& url);

  // The preview TabContents (may be null).
  TabContentsWrapper* prerender_contents() const {
    return prerender_contents_.get();
  }

  TabContentsWrapper* ReleasePrerenderContents();

  // Sets the final status, calls OnDestroy and adds |this| to the
  // PrerenderManager's pending deletes list.
  void Destroy(FinalStatus reason);

  // Indicates whether to use the legacy code doing prerendering via
  // a RenderViewHost (false), or whether the new TabContents based prerendering
  // is to be used (true).
  // TODO(cbentzel): Remove once new approach looks stable.
  static bool UseTabContents() {
    return true;
  }

 protected:
  PrerenderContents(PrerenderManager* prerender_manager,
                    Profile* profile,
                    const GURL& url,
                    const GURL& referrer);

  const GURL& prerender_url() const { return prerender_url_; }

  NotificationRegistrar& notification_registrar() {
    return notification_registrar_;
  }

  // Called whenever a RenderViewHost is created for prerendering.  Only called
  // once the RenderViewHost has a RenderView and RenderWidgetHostView.
  virtual void OnRenderViewHostCreated(RenderViewHost* new_render_view_host);

 private:
  // Needs to be able to call the constructor.
  friend class PrerenderContentsFactoryImpl;

  friend class PrerenderRenderViewHostObserver;

  // Message handlers.
  void OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                         bool main_frame,
                                         const GURL& url);
  void OnUpdateFaviconURL(int32 page_id, const std::vector<FaviconURL>& urls);

  // Returns the RenderViewHost Delegate for this prerender.
  RenderViewHostDelegate* GetRenderViewHostDelegate();

  // Returns the ProcessMetrics for the render process, if it exists.
  base::ProcessMetrics* MaybeGetProcessMetrics();

  // The prerender manager owning this object.
  PrerenderManager* prerender_manager_;

  // The host for our HTML content.
  RenderViewHost* render_view_host_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;

  // The URL being prerendered.
  GURL prerender_url_;

  // The referrer.
  GURL referrer_;

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
  GURL icon_url_;
  NotificationRegistrar notification_registrar_;
  TabContentsObserver::Registrar tab_contents_observer_registrar_;

  // A vector of URLs that this prerendered page matches against.
  // This array can contain more than element as a result of redirects,
  // such as HTTP redirects or javascript redirects.
  std::vector<GURL> alias_urls_;

  bool has_stopped_loading_;

  FinalStatus final_status_;

  bool prerendering_has_started_;

  // Time at which we started to load the URL.  This is used to compute
  // the time elapsed from initiating a prerender until the time the
  // (potentially only partially) prerendered page is shown to the user.
  base::TimeTicks load_start_time_;

  // Process Metrics of the render process associated with the
  // RenderViewHost for this object.
  scoped_ptr<base::ProcessMetrics> process_metrics_;

  // The prerendered TabContents; may be null.
  scoped_ptr<TabContentsWrapper> prerender_contents_;

  scoped_ptr<PrerenderRenderViewHostObserver> render_view_host_observer_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderContents);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_CONTENTS_H_
