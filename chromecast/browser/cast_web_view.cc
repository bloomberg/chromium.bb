// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_view.h"

#include <utility>

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/renderer_preferences.h"
#include "ipc/ipc_message.h"
#include "net/base/net_errors.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chromecast/browser/android/cast_web_contents_activity.h"
#endif  // defined(OS_ANDROID)

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace chromecast {

namespace {
// The time (in milliseconds) we wait for after a page is closed (i.e.
// after an app is stopped) before we delete the corresponding WebContents.
constexpr int kWebContentsDestructionDelayInMs = 50;

std::unique_ptr<content::WebContents> CreateWebContents(
    content::BrowserContext* browser_context,
    scoped_refptr<content::SiteInstance> site_instance) {
  CHECK(display::Screen::GetScreen());
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();

  content::WebContents::CreateParams create_params(browser_context, NULL);
  create_params.routing_id = MSG_ROUTING_NONE;
  create_params.initial_size = display_size;
  create_params.site_instance = site_instance;
  content::WebContents* web_contents =
      content::WebContents::Create(create_params);

#if defined(OS_ANDROID)
  content::RendererPreferences* prefs = web_contents->GetMutableRendererPrefs();
  prefs->use_video_overlay_for_embedded_encrypted_video = true;
  web_contents->GetRenderViewHost()->SyncRendererPrefs();
#endif

  return base::WrapUnique(web_contents);
}

}  // namespace

CastWebView::CastWebView(Delegate* delegate,
                         content::BrowserContext* browser_context,
                         scoped_refptr<content::SiteInstance> site_instance,
                         bool transparent)
    : delegate_(delegate),
      browser_context_(browser_context),
      site_instance_(std::move(site_instance)),
      transparent_(transparent),
      window_(shell::CastContentWindow::Create(delegate)),
      web_contents_(CreateWebContents(browser_context_, site_instance_)),
      did_start_navigation_(false),
      weak_factory_(this) {
  DCHECK(delegate_);
  DCHECK(browser_context_);
  DCHECK(window_);
  content::WebContentsObserver::Observe(web_contents_.get());
  web_contents_->SetDelegate(this);

  if (transparent_)
    window_->SetTransparent();
}

CastWebView::~CastWebView() {}

void CastWebView::LoadUrl(GURL url) {
  web_contents_->GetController().LoadURL(url, content::Referrer(),
                                         ui::PAGE_TRANSITION_TYPED, "");
}

void CastWebView::ClosePage() {
  content::WebContentsObserver::Observe(nullptr);
  web_contents_->ClosePage();
}

void CastWebView::CloseContents(content::WebContents* source) {
  DCHECK_EQ(source, web_contents_.get());

  // We need to delay the deletion of web_contents_ to give (and guarantee) the
  // renderer enough time to finish 'onunload' handler (but we don't want to
  // wait any longer than that to delay the starting of next app).
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&CastWebView::DelayedCloseContents,
                            weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kWebContentsDestructionDelayInMs));
}

void CastWebView::DelayedCloseContents() {
  // Delete the WebContents object here so that the gfx surface will be
  // deleted as part of destroying RenderWidgetHostViewCast object.
  // We want to delete the surface before we start the next app because
  // the next app could be an external one whose Start() function would
  // destroy the primary gfx plane.
  web_contents_.reset();
  delegate_->OnPageStopped(net::OK);
}

void CastWebView::Show(CastWindowManager* window_manager) {
  DCHECK(window_manager);
  window_->ShowWebContents(web_contents_.get(), window_manager);
  web_contents_->Focus();
}

content::WebContents* CastWebView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  LOG(INFO) << "Change url: " << params.url;
  // If source is NULL which means current tab, use web_contents_ of this class.
  if (!source)
    source = web_contents_.get();
  DCHECK_EQ(source, web_contents_.get());
  // We don't want to create another web_contents. Load url only when source is
  // specified.
  source->GetController().LoadURL(params.url, params.referrer,
                                  params.transition, params.extra_headers);
  return source;
}

void CastWebView::LoadingStateChanged(content::WebContents* source,
                                      bool to_different_document) {
  delegate_->OnLoadingStateChanged(source->IsLoading());
}

void CastWebView::ActivateContents(content::WebContents* contents) {
  DCHECK_EQ(contents, web_contents_.get());
  contents->GetRenderViewHost()->GetWidget()->Focus();
}

#if defined(OS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
CastWebView::GetContentVideoViewEmbedder() {
  DCHECK(web_contents_);
  auto* activity = shell::CastWebContentsActivity::Get(web_contents_.get());
  return activity->GetContentVideoViewEmbedder();
}
#endif  // defined(OS_ANDROID)

void CastWebView::RenderProcessGone(base::TerminationStatus status) {
  LOG(INFO) << "APP_ERROR_CHILD_PROCESS_CRASHED";
  delegate_->OnPageStopped(net::ERR_UNEXPECTED);
}

void CastWebView::RenderViewCreated(content::RenderViewHost* render_view_host) {
  content::RenderWidgetHostView* view =
      render_view_host->GetWidget()->GetView();
  if (view) {
    view->SetBackgroundColor(transparent_ ? SK_ColorTRANSPARENT
                                          : SK_ColorBLACK);
  }
}

void CastWebView::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // If the navigation was not committed, it means either the page was a
  // download or error 204/205, or the navigation never left the previous
  // URL. Ignore these navigations.
  if (!navigation_handle->HasCommitted()) {
    LOG(WARNING) << "Navigation did not commit: url="
                 << navigation_handle->GetURL();
    return;
  }

  net::Error error_code = navigation_handle->GetNetErrorCode();
  if (!navigation_handle->IsErrorPage())
    return;

  // If we abort errors in an iframe, it can create a really confusing
  // and fragile user experience.  Rather than create a list of errors
  // that are most likely to occur, we ignore all of them for now.
  if (!navigation_handle->IsInMainFrame()) {
    LOG(ERROR) << "Got error on sub-iframe: url=" << navigation_handle->GetURL()
               << ", error=" << error_code
               << ", description=" << net::ErrorToShortString(error_code);
    return;
  }

  LOG(ERROR) << "Got error on navigation: url=" << navigation_handle->GetURL()
             << ", error_code=" << error_code
             << ", description= " << net::ErrorToShortString(error_code);
  delegate_->OnPageStopped(error_code);
}

void CastWebView::DidFailLoad(content::RenderFrameHost* render_frame_host,
                              const GURL& validated_url,
                              int error_code,
                              const base::string16& error_description,
                              bool was_ignored_by_handler) {
  // Only report an error if we are the main frame.  See b/8433611.
  if (render_frame_host->GetParent()) {
    LOG(ERROR) << "Got error on sub-iframe: url=" << validated_url.spec()
               << ", error=" << error_code;
  } else if (error_code == net::ERR_ABORTED) {
    // ERR_ABORTED means download was aborted by the app, typically this happens
    // when flinging URL for direct playback, the initial URLRequest gets
    // cancelled/aborted and then the same URL is requested via the buffered
    // data source for media::Pipeline playback.
    LOG(INFO) << "Load canceled: url=" << validated_url.spec();
  } else {
    LOG(ERROR) << "Got error on load: url=" << validated_url.spec()
               << ", error_code=" << error_code;
    delegate_->OnPageStopped(error_code);
  }
}

void CastWebView::DidFirstVisuallyNonEmptyPaint() {
  metrics::CastMetricsHelper::GetInstance()->LogTimeToFirstPaint();
}

void CastWebView::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (did_start_navigation_) {
    return;
  }
  did_start_navigation_ = true;

#if defined(USE_AURA)
  // Resize window
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();
  aura::Window* content_window = web_contents()->GetNativeView();
  content_window->SetBounds(
      gfx::Rect(display_size.width(), display_size.height()));
#endif
}

void CastWebView::MediaStartedPlaying(const MediaPlayerInfo& media_info,
                                      const MediaPlayerId& id) {
  metrics::CastMetricsHelper::GetInstance()->LogMediaPlay();
}

void CastWebView::MediaStoppedPlaying(const MediaPlayerInfo& media_info,
                                      const MediaPlayerId& id) {
  metrics::CastMetricsHelper::GetInstance()->LogMediaPause();
}

}  // namespace chromecast
