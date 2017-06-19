// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/answer_card/answer_card_web_contents.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

class SearchAnswerWebView : public views::WebView {
 public:
  explicit SearchAnswerWebView(content::BrowserContext* browser_context)
      : WebView(browser_context) {}

  // views::WebView overrides:
  void VisibilityChanged(View* starting_from, bool is_visible) override {
    WebView::VisibilityChanged(starting_from, is_visible);

    if (GetWidget() && GetWidget()->IsVisible() && IsDrawn()) {
      if (shown_time_.is_null())
        shown_time_ = base::TimeTicks::Now();
    } else {
      if (!shown_time_.is_null()) {
        UMA_HISTOGRAM_MEDIUM_TIMES("SearchAnswer.AnswerVisibleTime",
                                   base::TimeTicks::Now() - shown_time_);
        shown_time_ = base::TimeTicks();
      }
    }
  }

  const char* GetClassName() const override { return "SearchAnswerWebView"; }

 private:
  // Time when the answer became visible to the user.
  base::TimeTicks shown_time_;

  DISALLOW_COPY_AND_ASSIGN(SearchAnswerWebView);
};

}  // namespace

AnswerCardWebContents::AnswerCardWebContents(Profile* profile)
    : web_view_(base::MakeUnique<SearchAnswerWebView>(profile)),
      web_contents_(
          content::WebContents::Create(content::WebContents::CreateParams(
              profile,
              content::SiteInstance::Create(profile)))),
      mouse_event_callback_(base::Bind(&AnswerCardWebContents::HandleMouseEvent,
                                       base::Unretained(this))) {
  content::RendererPreferences* renderer_prefs =
      web_contents_->GetMutableRendererPrefs();
  renderer_prefs->can_accept_load_drops = false;
  // We need the OpenURLFromTab() to get called.
  renderer_prefs->browser_handles_all_top_level_requests = true;
  web_contents_->GetRenderViewHost()->SyncRendererPrefs();

  Observe(web_contents_.get());
  web_contents_->SetDelegate(this);
  web_view_->set_owned_by_client();
  web_view_->SetWebContents(web_contents_.get());

  // Make the webview transparent since it's going to be shown on top of a
  // highlightable button.
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_contents_.get(), SK_ColorTRANSPARENT);

  content::RenderViewHost* const rvh = web_contents_->GetRenderViewHost();
  if (rvh)
    AttachToHost(rvh->GetWidget());
}

AnswerCardWebContents::~AnswerCardWebContents() {
  DetachFromHost();
}

void AnswerCardWebContents::LoadURL(const GURL& url) {
  content::NavigationController::LoadURLParams load_params(url);
  load_params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  load_params.should_clear_history_list = true;
  web_contents_->GetController().LoadURLWithParams(load_params);

  web_contents_->GetRenderViewHost()->EnablePreferredSizeMode();
}

bool AnswerCardWebContents::IsLoading() const {
  return web_contents_->IsLoading();
}

views::View* AnswerCardWebContents::GetView() {
  return web_view_.get();
}

void AnswerCardWebContents::UpdatePreferredSize(
    content::WebContents* web_contents,
    const gfx::Size& pref_size) {
  delegate()->UpdatePreferredSize(pref_size);
  web_view_->SetPreferredSize(pref_size);
}

content::WebContents* AnswerCardWebContents::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (!params.user_gesture)
    return WebContentsDelegate::OpenURLFromTab(source, params);

  return delegate()->OpenURLFromTab(params);
}

bool AnswerCardWebContents::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Disable showing the menu.
  return true;
}

void AnswerCardWebContents::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  delegate()->DidFinishNavigation(navigation_handle);
}

void AnswerCardWebContents::DidStopLoading() {
  delegate()->DidStopLoading();
}

void AnswerCardWebContents::DidGetUserInteraction(
    const blink::WebInputEvent::Type type) {
  base::RecordAction(base::UserMetricsAction("SearchAnswer_UserInteraction"));
}

void AnswerCardWebContents::RenderViewCreated(content::RenderViewHost* host) {
  if (!host_)
    AttachToHost(host->GetWidget());
}

void AnswerCardWebContents::RenderViewDeleted(content::RenderViewHost* host) {
  if (host->GetWidget() == host_)
    DetachFromHost();
}

void AnswerCardWebContents::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  if ((old_host && old_host->GetWidget() == host_) || (!old_host && !host_)) {
    DetachFromHost();
    AttachToHost(new_host->GetWidget());
  }
}

bool AnswerCardWebContents::HandleMouseEvent(
    const blink::WebMouseEvent& event) {
  switch (event.GetType()) {
    case blink::WebInputEvent::kMouseMove:
    case blink::WebInputEvent::kMouseEnter:
      SetIsMouseInView(true);
      break;
    case blink::WebInputEvent::kMouseLeave:
      SetIsMouseInView(false);
      break;
    default:
      break;
  }

  return false;
}

void AnswerCardWebContents::AttachToHost(content::RenderWidgetHost* host) {
  host_ = host;
  host->AddMouseEventCallback(mouse_event_callback_);
}

void AnswerCardWebContents::DetachFromHost() {
  if (!host_)
    return;

  host_->RemoveMouseEventCallback(mouse_event_callback_);
  host_ = nullptr;
}

}  // namespace app_list
