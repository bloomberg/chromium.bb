// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/page_handler.h"

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/page_navigation_throttle.h"
#include "content/browser/devtools/protocol/color_picker.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"

namespace content {
namespace protocol {

namespace {

static const char kPng[] = "png";
static const char kJpeg[] = "jpeg";
static int kDefaultScreenshotQuality = 80;
static int kFrameRetryDelayMs = 100;
static int kCaptureRetryLimit = 2;
static int kMaxScreencastFramesInFlight = 2;

std::string EncodeImage(const gfx::Image& image,
                        const std::string& format,
                        int quality) {
  DCHECK(!image.IsEmpty());

  scoped_refptr<base::RefCountedMemory> data;
  if (format == kPng) {
    data = image.As1xPNGBytes();
  } else if (format == kJpeg) {
    scoped_refptr<base::RefCountedBytes> bytes(new base::RefCountedBytes());
    if (gfx::JPEG1xEncodedDataFromImage(image, quality, &bytes->data()))
      data = bytes;
  }

  if (!data || !data->front())
    return std::string();

  std::string base_64_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(data->front()),
                        data->size()),
      &base_64_data);

  return base_64_data;
}

}  // namespace

PageHandler::PageHandler()
    : DevToolsDomainHandler(Page::Metainfo::domainName),
      enabled_(false),
      screencast_enabled_(false),
      screencast_quality_(kDefaultScreenshotQuality),
      screencast_max_width_(-1),
      screencast_max_height_(-1),
      capture_every_nth_frame_(1),
      capture_retry_count_(0),
      has_compositor_frame_metadata_(false),
      session_id_(0),
      frame_counter_(0),
      frames_in_flight_(0),
      color_picker_(new ColorPicker(
          base::Bind(&PageHandler::OnColorPicked, base::Unretained(this)))),
      navigation_throttle_enabled_(false),
      next_navigation_id_(0),
      host_(nullptr),
      weak_factory_(this) {}

PageHandler::~PageHandler() {
}

// static
PageHandler* PageHandler::FromSession(DevToolsSession* session) {
  return static_cast<PageHandler*>(
      session->GetHandlerByName(Page::Metainfo::domainName));
}

void PageHandler::SetRenderFrameHost(RenderFrameHostImpl* host) {
  if (host_ == host)
    return;

  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (widget_host) {
    registrar_.Remove(
        this,
        content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
        content::Source<RenderWidgetHost>(widget_host));
  }

  host_ = host;
  widget_host = host_ ? host_->GetRenderWidgetHost() : nullptr;
  color_picker_->SetRenderWidgetHost(widget_host);

  if (widget_host) {
    registrar_.Add(
        this,
        content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
        content::Source<RenderWidgetHost>(widget_host));
  }
}

void PageHandler::Wire(UberDispatcher* dispatcher) {
  frontend_.reset(new Page::Frontend(dispatcher->channel()));
  Page::Dispatcher::wire(dispatcher, this);
}

void PageHandler::OnSwapCompositorFrame(
    cc::CompositorFrameMetadata frame_metadata) {
  last_compositor_frame_metadata_ = std::move(frame_metadata);
  has_compositor_frame_metadata_ = true;

  if (screencast_enabled_)
    InnerSwapCompositorFrame();
  color_picker_->OnSwapCompositorFrame();
}

void PageHandler::OnSynchronousSwapCompositorFrame(
    cc::CompositorFrameMetadata frame_metadata) {
  if (has_compositor_frame_metadata_) {
    last_compositor_frame_metadata_ =
        std::move(next_compositor_frame_metadata_);
  } else {
    last_compositor_frame_metadata_ = frame_metadata.Clone();
  }
  next_compositor_frame_metadata_ = std::move(frame_metadata);

  has_compositor_frame_metadata_ = true;

  if (screencast_enabled_)
    InnerSwapCompositorFrame();
  color_picker_->OnSwapCompositorFrame();
}

void PageHandler::Observe(int type,
                          const NotificationSource& source,
                          const NotificationDetails& details) {
  if (!screencast_enabled_)
    return;
  DCHECK(type == content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED);
  bool visible = *Details<bool>(details).ptr();
  NotifyScreencastVisibility(visible);
}

void PageHandler::DidAttachInterstitialPage() {
  if (!enabled_)
    return;
  frontend_->InterstitialShown();
}

void PageHandler::DidDetachInterstitialPage() {
  if (!enabled_)
    return;
  frontend_->InterstitialHidden();
}

Response PageHandler::Enable() {
  enabled_ = true;
  if (GetWebContents() && GetWebContents()->ShowingInterstitialPage())
    frontend_->InterstitialShown();
  return Response::FallThrough();
}

Response PageHandler::Disable() {
  enabled_ = false;
  screencast_enabled_ = false;
  SetControlNavigations(false);
  color_picker_->SetEnabled(false);
  return Response::FallThrough();
}

Response PageHandler::Reload(Maybe<bool> bypassCache,
                             Maybe<std::string> script_to_evaluate_on_load) {
  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError();

  if (web_contents->IsCrashed() ||
      (web_contents->GetController().GetVisibleEntry() &&
       web_contents->GetController().GetVisibleEntry()->IsViewSourceMode())) {
    web_contents->GetController().Reload(bypassCache.fromMaybe(false)
                                             ? ReloadType::BYPASSING_CACHE
                                             : ReloadType::NORMAL,
                                         false);
    return Response::OK();
  } else {
    // Handle reload in renderer except for crashed and view source mode.
    return Response::FallThrough();
  }
}

Response PageHandler::Navigate(const std::string& url,
                               Maybe<std::string> referrer,
                               Page::FrameId* frame_id) {
  GURL gurl(url);
  if (!gurl.is_valid())
    return Response::Error("Cannot navigate to invalid URL");

  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError();

  web_contents->GetController().LoadURL(
      gurl,
      Referrer(GURL(referrer.fromMaybe("")), blink::WebReferrerPolicyDefault),
      ui::PAGE_TRANSITION_TYPED, std::string());
  return Response::FallThrough();
}

Response PageHandler::GetNavigationHistory(
    int* current_index,
    std::unique_ptr<NavigationEntries>* entries) {
  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError();

  NavigationController& controller = web_contents->GetController();
  *current_index = controller.GetCurrentEntryIndex();
  *entries = NavigationEntries::create();
  for (int i = 0; i != controller.GetEntryCount(); ++i) {
    (*entries)->addItem(Page::NavigationEntry::Create()
        .SetId(controller.GetEntryAtIndex(i)->GetUniqueID())
        .SetUrl(controller.GetEntryAtIndex(i)->GetURL().spec())
        .SetTitle(base::UTF16ToUTF8(controller.GetEntryAtIndex(i)->GetTitle()))
        .Build());
  }
  return Response::OK();
}

Response PageHandler::NavigateToHistoryEntry(int entry_id) {
  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError();

  NavigationController& controller = web_contents->GetController();
  for (int i = 0; i != controller.GetEntryCount(); ++i) {
    if (controller.GetEntryAtIndex(i)->GetUniqueID() == entry_id) {
      controller.GoToIndex(i);
      return Response::OK();
    }
  }

  return Response::InvalidParams("No entry with passed id");
}

void PageHandler::CaptureScreenshot(
    Maybe<std::string> format,
    Maybe<int> quality,
    std::unique_ptr<CaptureScreenshotCallback> callback) {
  if (!host_ || !host_->GetRenderWidgetHost()) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  std::string screenshot_format = format.fromMaybe(kPng);
  int screenshot_quality = quality.fromMaybe(kDefaultScreenshotQuality);

  host_->GetRenderWidgetHost()->GetSnapshotFromBrowser(
      base::Bind(&PageHandler::ScreenshotCaptured, weak_factory_.GetWeakPtr(),
                 base::Passed(std::move(callback)), screenshot_format,
                 screenshot_quality));
}

void PageHandler::PrintToPDF(std::unique_ptr<PrintToPDFCallback> callback) {
  callback->sendFailure(Response::Error("PrintToPDF is not implemented"));
  return;
}

Response PageHandler::StartScreencast(Maybe<std::string> format,
                                      Maybe<int> quality,
                                      Maybe<int> max_width,
                                      Maybe<int> max_height,
                                      Maybe<int> every_nth_frame) {
  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (!widget_host)
    return Response::InternalError();

  screencast_enabled_ = true;
  screencast_format_ = format.fromMaybe(kPng);
  screencast_quality_ = quality.fromMaybe(kDefaultScreenshotQuality);
  if (screencast_quality_ < 0 || screencast_quality_ > 100)
    screencast_quality_ = kDefaultScreenshotQuality;
  screencast_max_width_ = max_width.fromMaybe(-1);
  screencast_max_height_ = max_height.fromMaybe(-1);
  ++session_id_;
  frame_counter_ = 0;
  frames_in_flight_ = 0;
  capture_every_nth_frame_ = every_nth_frame.fromMaybe(1);

  bool visible = !widget_host->is_hidden();
  NotifyScreencastVisibility(visible);
  if (visible) {
    if (has_compositor_frame_metadata_) {
      InnerSwapCompositorFrame();
    } else {
      widget_host->Send(new ViewMsg_ForceRedraw(widget_host->GetRoutingID(),
                                                ui::LatencyInfo()));
    }
  }
  return Response::FallThrough();
}

Response PageHandler::StopScreencast() {
  screencast_enabled_ = false;
  return Response::FallThrough();
}

Response PageHandler::ScreencastFrameAck(int session_id) {
  if (session_id == session_id_)
    --frames_in_flight_;
  return Response::OK();
}

Response PageHandler::HandleJavaScriptDialog(bool accept,
                                             Maybe<std::string> prompt_text) {
  base::string16 prompt_override;
  if (prompt_text.isJust())
    prompt_override = base::UTF8ToUTF16(prompt_text.fromJust());

  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError();

  JavaScriptDialogManager* manager =
      web_contents->GetDelegate()->GetJavaScriptDialogManager(web_contents);
  if (manager && manager->HandleJavaScriptDialog(
          web_contents, accept,
          prompt_text.isJust() ? &prompt_override : nullptr)) {
    return Response::OK();
  }

  return Response::Error("Could not handle JavaScript dialog");
}

Response PageHandler::SetColorPickerEnabled(bool enabled) {
  if (!host_)
    return Response::InternalError();

  color_picker_->SetEnabled(enabled);
  return Response::OK();
}

Response PageHandler::RequestAppBanner() {
  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError();
  web_contents->GetDelegate()->RequestAppBannerFromDevTools(web_contents);
  return Response::OK();
}

Response PageHandler::SetControlNavigations(bool enabled) {
  navigation_throttle_enabled_ = enabled;
  // We don't own the page PageNavigationThrottles so we can't delete them, but
  // we can turn them into NOPs.
  for (auto& pair : navigation_throttles_) {
    pair.second->AlwaysProceed();
  }
  navigation_throttles_.clear();
  return Response::OK();
}

Response PageHandler::ProcessNavigation(const std::string& response,
                                        int navigation_id) {
  auto it = navigation_throttles_.find(navigation_id);
  if (it == navigation_throttles_.end())
    return Response::InvalidParams("Unknown navigation id");

  if (response == Page::NavigationResponseEnum::Proceed) {
    it->second->Resume();
    return Response::OK();
  } else if (response == Page::NavigationResponseEnum::Cancel) {
    it->second->CancelDeferredNavigation(content::NavigationThrottle::CANCEL);
    return Response::OK();
  } else if (response == Page::NavigationResponseEnum::CancelAndIgnore) {
    it->second->CancelDeferredNavigation(
        content::NavigationThrottle::CANCEL_AND_IGNORE);
    return Response::OK();
  }

  return Response::InvalidParams("Unrecognized response");
}

std::unique_ptr<PageNavigationThrottle>
PageHandler::CreateThrottleForNavigation(NavigationHandle* navigation_handle) {
  if (!navigation_throttle_enabled_)
    return nullptr;

  std::unique_ptr<PageNavigationThrottle> throttle(new PageNavigationThrottle(
      weak_factory_.GetWeakPtr(), next_navigation_id_, navigation_handle));
  navigation_throttles_[next_navigation_id_++] = throttle.get();
  return throttle;
}

void PageHandler::OnPageNavigationThrottleDisposed(int navigation_id) {
  DCHECK(navigation_throttles_.find(navigation_id) !=
         navigation_throttles_.end());
  navigation_throttles_.erase(navigation_id);
}

void PageHandler::NavigationRequested(const PageNavigationThrottle* throttle) {
  NavigationHandle* navigation_handle = throttle->navigation_handle();
  frontend_->NavigationRequested(
      navigation_handle->IsInMainFrame(),
      navigation_handle->WasServerRedirect(),
      throttle->navigation_id(),
      navigation_handle->GetURL().spec());
}

WebContentsImpl* PageHandler::GetWebContents() {
  return host_ ?
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(host_)) :
      nullptr;
}

void PageHandler::NotifyScreencastVisibility(bool visible) {
  if (visible)
    capture_retry_count_ = kCaptureRetryLimit;
  frontend_->ScreencastVisibilityChanged(visible);
}

void PageHandler::InnerSwapCompositorFrame() {
  if (!host_ || !host_->GetView())
    return;

  if (frames_in_flight_ > kMaxScreencastFramesInFlight)
    return;

  if (++frame_counter_ % capture_every_nth_frame_)
    return;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      host_->GetView());
  // TODO(vkuzkokov): do not use previous frame metadata.
  // TODO(miu): RWHV to provide an API to provide actual rendering size.
  // http://crbug.com/73362
  cc::CompositorFrameMetadata& metadata = last_compositor_frame_metadata_;

  gfx::SizeF viewport_size_dip = gfx::ScaleSize(
      metadata.scrollable_viewport_size, metadata.page_scale_factor);
  gfx::SizeF screen_size_dip =
      gfx::ScaleSize(gfx::SizeF(view->GetPhysicalBackingSize()),
                     1 / metadata.device_scale_factor);

  content::ScreenInfo screen_info;
  GetWebContents()->GetView()->GetScreenInfo(&screen_info);
  double device_scale_factor = screen_info.device_scale_factor;
  double scale = 1;

  if (screencast_max_width_ > 0) {
    double max_width_dip = screencast_max_width_ / device_scale_factor;
    scale = std::min(scale, max_width_dip / screen_size_dip.width());
  }
  if (screencast_max_height_ > 0) {
    double max_height_dip = screencast_max_height_ / device_scale_factor;
    scale = std::min(scale, max_height_dip / screen_size_dip.height());
  }

  if (scale <= 0)
    scale = 0.1;

  gfx::Size snapshot_size_dip(gfx::ToRoundedSize(
      gfx::ScaleSize(viewport_size_dip, scale)));

  if (snapshot_size_dip.width() > 0 && snapshot_size_dip.height() > 0) {
    view->CopyFromSurface(
        gfx::Rect(), snapshot_size_dip,
        base::Bind(&PageHandler::ScreencastFrameCaptured,
                   weak_factory_.GetWeakPtr(),
                   base::Passed(last_compositor_frame_metadata_.Clone())),
        kN32_SkColorType);
    frames_in_flight_++;
  }
}

void PageHandler::ScreencastFrameCaptured(cc::CompositorFrameMetadata metadata,
                                          const SkBitmap& bitmap,
                                          ReadbackResponse response) {
  if (response != READBACK_SUCCESS) {
    if (capture_retry_count_) {
      --capture_retry_count_;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, base::Bind(&PageHandler::InnerSwapCompositorFrame,
                                weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kFrameRetryDelayMs));
    }
    --frames_in_flight_;
    return;
  }
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits().WithShutdownBehavior(
                     base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN),
      base::Bind(&EncodeImage, gfx::Image::CreateFrom1xBitmap(bitmap),
                 screencast_format_, screencast_quality_),
      base::Bind(&PageHandler::ScreencastFrameEncoded,
                 weak_factory_.GetWeakPtr(), base::Passed(&metadata),
                 base::Time::Now()));
}

void PageHandler::ScreencastFrameEncoded(cc::CompositorFrameMetadata metadata,
                                         const base::Time& timestamp,
                                         const std::string& data) {
  // Consider metadata empty in case it has no device scale factor.
  if (metadata.device_scale_factor == 0 || !host_ || data.empty()) {
    --frames_in_flight_;
    return;
  }

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      host_->GetView());
  if (!view) {
    --frames_in_flight_;
    return;
  }

  gfx::SizeF screen_size_dip =
      gfx::ScaleSize(gfx::SizeF(view->GetPhysicalBackingSize()),
                     1 / metadata.device_scale_factor);
  std::unique_ptr<Page::ScreencastFrameMetadata> param_metadata =
      Page::ScreencastFrameMetadata::Create()
          .SetPageScaleFactor(metadata.page_scale_factor)
          .SetOffsetTop(metadata.top_controls_height *
                        metadata.top_controls_shown_ratio)
          .SetDeviceWidth(screen_size_dip.width())
          .SetDeviceHeight(screen_size_dip.height())
          .SetScrollOffsetX(metadata.root_scroll_offset.x())
          .SetScrollOffsetY(metadata.root_scroll_offset.y())
          .SetTimestamp(timestamp.ToDoubleT())
          .Build();
  frontend_->ScreencastFrame(data, std::move(param_metadata), session_id_);
}

void PageHandler::ScreenshotCaptured(
    std::unique_ptr<CaptureScreenshotCallback> callback,
    const std::string& format,
    int quality,
    const gfx::Image& image) {
  if (image.IsEmpty()) {
    callback->sendFailure(Response::Error("Unable to capture screenshot"));
    return;
  }

  callback->sendSuccess(EncodeImage(image, format, quality));
}

void PageHandler::OnColorPicked(int r, int g, int b, int a) {
  frontend_->ColorPicked(
      DOM::RGBA::Create().SetR(r).SetG(g).SetB(b).SetA(a).Build());
}

Response PageHandler::StopLoading() {
  WebContentsImpl* web_contents = GetWebContents();
  if (!web_contents)
    return Response::InternalError();
  web_contents->Stop();
  return Response::OK();
}

}  // namespace protocol
}  // namespace content
