// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_controller.h"

#include <set>

#include "base/json/json_writer.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "chromecast/browser/webview/webview_navigation_throttle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/web_preferences.h"

namespace chromecast {

namespace {

const void* kWebviewResponseUserDataKey = &kWebviewResponseUserDataKey;

class WebviewUserData : public base::SupportsUserData::Data {
 public:
  explicit WebviewUserData(WebviewController* controller);
  ~WebviewUserData() override;

  std::unique_ptr<Data> Clone() override;

  WebviewController* controller() const { return controller_; }

 private:
  WebviewController* controller_;
};

}  // namespace

WebviewController::WebviewController(content::BrowserContext* browser_context,
                                     Client* client,
                                     bool enabled_for_dev)
    : WebContentController(client), enabled_for_dev_(enabled_for_dev) {
  content::WebContents::CreateParams create_params(browser_context, nullptr);
  contents_ = content::WebContents::Create(create_params);
  contents_->SetUserData(kWebviewResponseUserDataKey,
                         std::make_unique<WebviewUserData>(this));
  CastWebContents::InitParams cast_contents_init;
  cast_contents_init.is_root_window = true;
  cast_contents_init.enabled_for_dev = enabled_for_dev;
  cast_contents_init.delegate = weak_ptr_factory_.GetWeakPtr();
  cast_web_contents_ = std::make_unique<CastWebContentsImpl>(
      contents_.get(), cast_contents_init);
  cast_web_contents_->AddObserver(this);

  content::WebContentsObserver::Observe(contents_.get());

  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  auto ax_id = contents_->GetMainFrame()->GetAXTreeID().ToString();
  response->mutable_create_response()
      ->mutable_accessibility_info()
      ->set_ax_tree_id(ax_id);
  client->EnqueueSend(std::move(response));
}

WebviewController::~WebviewController() {
  cast_web_contents_->RemoveObserver(this);
}

std::unique_ptr<content::NavigationThrottle>
WebviewController::MaybeGetNavigationThrottle(
    content::NavigationHandle* handle) {
  auto* web_contents = handle->GetWebContents();
  auto* webview_user_data = static_cast<WebviewUserData*>(
      web_contents->GetUserData(kWebviewResponseUserDataKey));
  if (webview_user_data &&
      webview_user_data->controller()->has_navigation_delegate_) {
    return std::make_unique<WebviewNavigationThrottle>(
        handle,
        webview_user_data->controller()->weak_ptr_factory_.GetWeakPtr());
  }
  return nullptr;
}

void WebviewController::ProcessRequest(const webview::WebviewRequest& request) {
  switch (request.type_case()) {
    case webview::WebviewRequest::kNavigate:
      if (request.has_navigate()) {
        LOG(INFO) << "Navigate webview to " << request.navigate().url();
        stopped_ = false;
        cast_web_contents_->LoadUrl(GURL(request.navigate().url()));
      } else {
        client_->OnError("navigate() not supplied");
      }
      break;

    case webview::WebviewRequest::kStopPage:
      if (request.has_stop_page()) {
        cast_web_contents_->Stop(request.stop_page().error_code());
      } else {
        client_->OnError("stop_page() not supplied");
      }
      break;

    case webview::WebviewRequest::kNavigationDecision:
      if (current_navigation_throttle_) {
        current_navigation_throttle_->ProcessNavigationDecision(
            request.navigation_decision());
        current_navigation_throttle_ = nullptr;
      }
      break;

    case webview::WebviewRequest::kUpdateSettings:
      if (request.has_update_settings()) {
        HandleUpdateSettings(request.update_settings());
      } else {
        client_->OnError("update_settings() not supplied");
      }
      break;

    default:
      WebContentController::ProcessRequest(request);
      break;
  }
}

void WebviewController::HandleUpdateSettings(
    const webview::UpdateSettingsRequest& request) {
  content::WebContents* contents = GetWebContents();
  content::WebPreferences prefs =
      contents->GetRenderViewHost()->GetWebkitPreferences();
  prefs.javascript_enabled = request.javascript_enabled();
  contents->GetRenderViewHost()->UpdateWebkitPreferences(prefs);

  has_navigation_delegate_ = request.has_navigation_delegate();

  CastWebContents::FromWebContents(contents)->SetEnabledForRemoteDebugging(
      request.debugging_enabled() || enabled_for_dev_);

  if (request.has_user_agent() &&
      request.user_agent().type_case() == webview::UserAgent::kValue) {
    contents->SetUserAgentOverride(
        blink::UserAgentOverride::UserAgentOnly(request.user_agent().value()),
        true);
  }
}

void WebviewController::DidFirstVisuallyNonEmptyPaint() {
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_url(contents_->GetURL().spec());
    event->set_current_page_state(current_state());
    event->set_did_first_visually_non_empty_paint(true);
    client_->EnqueueSend(std::move(response));
  }
}

void WebviewController::SendNavigationEvent(
    WebviewNavigationThrottle* throttle,
    content::NavigationHandle* navigation_handle) {
  DCHECK(!current_navigation_throttle_);
  DCHECK(navigation_handle);
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  auto* navigation_event = response->mutable_navigation_event();

  navigation_event->set_url(navigation_handle->GetURL().spec());
  navigation_event->set_is_for_main_frame(navigation_handle->IsInMainFrame());
  navigation_event->set_is_renderer_initiated(
      navigation_handle->IsRendererInitiated());
  navigation_event->set_is_same_document(navigation_handle->IsSameDocument());
  navigation_event->set_has_user_gesture(navigation_handle->HasUserGesture());
  navigation_event->set_was_server_redirect(
      navigation_handle->WasServerRedirect());
  navigation_event->set_is_post(navigation_handle->IsPost());
  navigation_event->set_was_initiated_by_link_click(
      navigation_handle->WasInitiatedByLinkClick());

  current_navigation_throttle_ = throttle;
  client_->EnqueueSend(std::move(response));
}

void WebviewController::OnNavigationThrottleDestroyed(
    WebviewNavigationThrottle* throttle) {
  if (current_navigation_throttle_ == throttle)
    current_navigation_throttle_ = nullptr;
}

void WebviewController::ClosePage() {
  cast_web_contents_->ClosePage();
}

content::WebContents* WebviewController::GetWebContents() {
  return contents_.get();
}

webview::AsyncPageEvent_State WebviewController::current_state() {
  // The PB enum is defined identically.
  return static_cast<webview::AsyncPageEvent_State>(
      cast_web_contents_->page_state());
}

void WebviewController::OnPageStateChanged(CastWebContents* cast_web_contents) {
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_url(contents_->GetURL().spec());
    event->set_current_page_state(current_state());
    client_->EnqueueSend(std::move(response));
  }
}

void WebviewController::OnPageStopped(CastWebContents* cast_web_contents,
                                      int error_code) {
  stopped_ = true;
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_url(contents_->GetURL().spec());
    event->set_current_page_state(current_state());
    event->set_stopped_error_code(error_code);
    event->set_stopped_error_description(net::ErrorToShortString(error_code));
    client_->EnqueueSend(std::move(response));
  } else {
    // Can't destroy in an observer callback, so post a task to do it.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }
}

void WebviewController::ResourceLoadFailed(CastWebContents* cast_web_contents) {
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_url(contents_->GetURL().spec());
    event->set_current_page_state(current_state());
    event->set_resource_load_failed(true);
    client_->EnqueueSend(std::move(response));
  }
}

void WebviewController::Destroy() {
  // This webview is now abandoned and should close.
  client_ = nullptr;
  js_channels_.reset();
  if (stopped_) {
    // If the page has been stopped this can be deleted immediately.
    delete this;
  } else {
    // This will eventually call OnPageStopped.
    cast_web_contents_->ClosePage();
  }
}

WebviewUserData::WebviewUserData(WebviewController* controller)
    : controller_(controller) {}

WebviewUserData::~WebviewUserData() = default;

std::unique_ptr<base::SupportsUserData::Data> WebviewUserData::Clone() {
  return std::make_unique<WebviewUserData>(controller_);
}

}  // namespace chromecast
