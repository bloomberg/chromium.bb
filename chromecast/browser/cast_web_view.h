// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_VIEW_H_
#define CHROMECAST_BROWSER_CAST_WEB_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_content_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class BrowserContext;
class RenderViewHost;
class SiteInstance;
}

namespace chromecast {

class CastWebContentsManager;
class CastWindowManager;

// A simplified interface for loading and displaying WebContents in cast_shell.
class CastWebView : content::WebContentsObserver, content::WebContentsDelegate {
 public:
  class Delegate : public shell::CastContentWindow::Delegate {
   public:
    // Called when the page has stopped. ie: A 404 occured when loading the page
    // or if the render process crashes. |error_code| will return a net::Error
    // describing the failure, or net::OK if the page closed naturally.
    virtual void OnPageStopped(int error_code) = 0;

    // Called during WebContentsDelegate::LoadingStateChanged.
    // |loading| indicates if web_contents_ IsLoading or not.
    virtual void OnLoadingStateChanged(bool loading) = 0;
  };

  // |delegate| and |browser_context| should outlive the lifetime of this
  // object.
  CastWebView(Delegate* delegate,
              CastWebContentsManager* web_contents_manager,
              content::BrowserContext* browser_context,
              scoped_refptr<content::SiteInstance> site_instance,
              bool transparent,
              bool allow_media_access);
  ~CastWebView() override;

  shell::CastContentWindow* window() const { return window_.get(); }

  content::WebContents* web_contents() const { return web_contents_.get(); }

  // Navigates to |url|. The loaded page will be preloaded if MakeVisible has
  // not been called on the object.
  void LoadUrl(GURL url);

  // Begins the close process for this page (ie. triggering document.onunload).
  // A consumer of the class can be notified when the process has been finished
  // via Delegate::OnPageStopped(). The page will be torn down after
  // |shutdown_delay| has elapsed, or sooner if required.
  void ClosePage(const base::TimeDelta& shutdown_delay);

  // Makes the page visible to the user.
  void Show(CastWindowManager* window_manager);

 private:
  // WebContentsObserver implementation:
  void RenderProcessGone(base::TerminationStatus status) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void MediaStartedPlaying(const MediaPlayerInfo& media_info,
                           const MediaPlayerId& id) override;
  void MediaStoppedPlaying(const MediaPlayerInfo& media_info,
                           const MediaPlayerId& id) override;

  // WebContentsDelegate implementation:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void CloseContents(content::WebContents* source) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void ActivateContents(content::WebContents* contents) override;
  bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override;
#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetContentVideoViewEmbedder()
      override;
#endif  // defined(OS_ANDROID)

  Delegate* const delegate_;
  CastWebContentsManager* const web_contents_manager_;
  content::BrowserContext* const browser_context_;
  const scoped_refptr<content::SiteInstance> site_instance_;
  const bool transparent_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<shell::CastContentWindow> window_;
  bool did_start_navigation_;
  base::TimeDelta shutdown_delay_;
  bool allow_media_access_;

  base::WeakPtrFactory<CastWebView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastWebView);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_VIEW_H_
