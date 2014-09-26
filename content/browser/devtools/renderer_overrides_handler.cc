// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/renderer_overrides_handler.h"

#include <map>
#include <string>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/devtools/devtools_tracing_handler.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "ipc/ipc_sender.h"
#include "net/base/net_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;

namespace content {

namespace {

static const char kPng[] = "png";
static const char kJpeg[] = "jpeg";
static int kDefaultScreenshotQuality = 80;
static int kFrameRateThresholdMs = 100;
static int kCaptureRetryLimit = 2;

}  // namespace

RendererOverridesHandler::RendererOverridesHandler()
    : page_domain_enabled_(false),
      has_last_compositor_frame_metadata_(false),
      capture_retry_count_(0),
      touch_emulation_enabled_(false),
      color_picker_enabled_(false),
      last_cursor_x_(-1),
      last_cursor_y_(-1),
      weak_factory_(this) {
  RegisterCommandHandler(
      devtools::DOM::setFileInputFiles::kName,
      base::Bind(
          &RendererOverridesHandler::GrantPermissionsForSetFileInputFiles,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Network::canEmulateNetworkConditions::kName,
      base::Bind(
          &RendererOverridesHandler::CanEmulateNetworkConditions,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Network::clearBrowserCache::kName,
      base::Bind(
          &RendererOverridesHandler::ClearBrowserCache,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Network::clearBrowserCookies::kName,
      base::Bind(
          &RendererOverridesHandler::ClearBrowserCookies,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::enable::kName,
      base::Bind(
          &RendererOverridesHandler::PageEnable, base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::disable::kName,
      base::Bind(
          &RendererOverridesHandler::PageDisable, base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::handleJavaScriptDialog::kName,
      base::Bind(
          &RendererOverridesHandler::PageHandleJavaScriptDialog,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::navigate::kName,
      base::Bind(
          &RendererOverridesHandler::PageNavigate,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::reload::kName,
      base::Bind(
          &RendererOverridesHandler::PageReload,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::getNavigationHistory::kName,
      base::Bind(
          &RendererOverridesHandler::PageGetNavigationHistory,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::navigateToHistoryEntry::kName,
      base::Bind(
          &RendererOverridesHandler::PageNavigateToHistoryEntry,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::captureScreenshot::kName,
      base::Bind(
          &RendererOverridesHandler::PageCaptureScreenshot,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::setTouchEmulationEnabled::kName,
      base::Bind(
          &RendererOverridesHandler::PageSetTouchEmulationEnabled,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::canEmulate::kName,
      base::Bind(
          &RendererOverridesHandler::PageCanEmulate,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::canScreencast::kName,
      base::Bind(
          &RendererOverridesHandler::PageCanScreencast,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::startScreencast::kName,
      base::Bind(
          &RendererOverridesHandler::PageStartScreencast,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::stopScreencast::kName,
      base::Bind(
          &RendererOverridesHandler::PageStopScreencast,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::queryUsageAndQuota::kName,
      base::Bind(
          &RendererOverridesHandler::PageQueryUsageAndQuota,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Page::setColorPickerEnabled::kName,
      base::Bind(
          &RendererOverridesHandler::PageSetColorPickerEnabled,
          base::Unretained(this)));
  RegisterCommandHandler(
      devtools::Input::emulateTouchFromMouseEvent::kName,
      base::Bind(
          &RendererOverridesHandler::InputEmulateTouchFromMouseEvent,
          base::Unretained(this)));
  mouse_event_callback_ = base::Bind(
      &RendererOverridesHandler::HandleMouseEvent,
      base::Unretained(this));
}

RendererOverridesHandler::~RendererOverridesHandler() {}

void RendererOverridesHandler::OnClientDetached() {
  touch_emulation_enabled_ = false;
  screencast_command_ = NULL;
  UpdateTouchEventEmulationState();
  SetColorPickerEnabled(false);
}

void RendererOverridesHandler::OnSwapCompositorFrame(
    const cc::CompositorFrameMetadata& frame_metadata) {
  last_compositor_frame_metadata_ = frame_metadata;
  has_last_compositor_frame_metadata_ = true;

  if (screencast_command_.get())
    InnerSwapCompositorFrame();
  if (color_picker_enabled_)
    UpdateColorPickerFrame();
}

void RendererOverridesHandler::OnVisibilityChanged(bool visible) {
  if (!screencast_command_.get())
    return;
  NotifyScreencastVisibility(visible);
}

void RendererOverridesHandler::SetRenderViewHost(
    RenderViewHostImpl* host) {
  host_ = host;
  if (!host)
    return;
  UpdateTouchEventEmulationState();
  if (color_picker_enabled_)
    host->AddMouseEventCallback(mouse_event_callback_);
}

void RendererOverridesHandler::ClearRenderViewHost() {
  if (host_)
    host_->RemoveMouseEventCallback(mouse_event_callback_);
  host_ = NULL;
  ResetColorPickerFrame();
}

void RendererOverridesHandler::DidAttachInterstitialPage() {
  if (page_domain_enabled_)
    SendNotification(devtools::Page::interstitialShown::kName, NULL);
}

void RendererOverridesHandler::DidDetachInterstitialPage() {
  if (page_domain_enabled_)
    SendNotification(devtools::Page::interstitialHidden::kName, NULL);
}

void RendererOverridesHandler::InnerSwapCompositorFrame() {
  if ((base::TimeTicks::Now() - last_frame_time_).InMilliseconds() <
          kFrameRateThresholdMs) {
    return;
  }

  if (!host_ || !host_->GetView())
    return;

  last_frame_time_ = base::TimeTicks::Now();

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      host_->GetView());
  // TODO(vkuzkokov): do not use previous frame metadata.
  cc::CompositorFrameMetadata& metadata = last_compositor_frame_metadata_;

  gfx::SizeF viewport_size_dip = gfx::ScaleSize(
      metadata.scrollable_viewport_size, metadata.page_scale_factor);
  gfx::SizeF screen_size_dip = gfx::ScaleSize(view->GetPhysicalBackingSize(),
                                              1 / metadata.device_scale_factor);

  std::string format;
  int quality = kDefaultScreenshotQuality;
  double scale = 1;
  double max_width = -1;
  double max_height = -1;
  base::DictionaryValue* params = screencast_command_->params();
  if (params) {
    params->GetString(devtools::Page::startScreencast::kParamFormat,
                      &format);
    params->GetInteger(devtools::Page::startScreencast::kParamQuality,
                       &quality);
    params->GetDouble(devtools::Page::startScreencast::kParamMaxWidth,
                      &max_width);
    params->GetDouble(devtools::Page::startScreencast::kParamMaxHeight,
                      &max_height);
  }

  blink::WebScreenInfo screen_info;
  view->GetScreenInfo(&screen_info);
  double device_scale_factor = screen_info.deviceScaleFactor;

  if (max_width > 0) {
    double max_width_dip = max_width / device_scale_factor;
    scale = std::min(scale, max_width_dip / screen_size_dip.width());
  }
  if (max_height > 0) {
    double max_height_dip = max_height / device_scale_factor;
    scale = std::min(scale, max_height_dip / screen_size_dip.height());
  }

  if (format.empty())
    format = kPng;
  if (quality < 0 || quality > 100)
    quality = kDefaultScreenshotQuality;
  if (scale <= 0)
    scale = 0.1;

  gfx::Size snapshot_size_dip(gfx::ToRoundedSize(
      gfx::ScaleSize(viewport_size_dip, scale)));

  if (snapshot_size_dip.width() > 0 && snapshot_size_dip.height() > 0) {
    gfx::Rect viewport_bounds_dip(gfx::ToRoundedSize(viewport_size_dip));
    view->CopyFromCompositingSurface(
        viewport_bounds_dip, snapshot_size_dip,
        base::Bind(&RendererOverridesHandler::ScreencastFrameCaptured,
                   weak_factory_.GetWeakPtr(),
                   format, quality, last_compositor_frame_metadata_),
        kN32_SkColorType);
  }
}

// DOM agent handlers  --------------------------------------------------------

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::GrantPermissionsForSetFileInputFiles(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  base::ListValue* file_list = NULL;
  const char* param =
      devtools::DOM::setFileInputFiles::kParamFiles;
  if (!params || !params->GetList(param, &file_list))
    return command->InvalidParamResponse(param);
  if (!host_)
    return NULL;

  for (size_t i = 0; i < file_list->GetSize(); ++i) {
    base::FilePath::StringType file;
    if (!file_list->GetString(i, &file))
      return command->InvalidParamResponse(param);
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        host_->GetProcess()->GetID(), base::FilePath(file));
  }
  return NULL;
}


// Network agent handlers  ----------------------------------------------------

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::CanEmulateNetworkConditions(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetBoolean(devtools::kResult, false);
  return command->SuccessResponse(result);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::ClearBrowserCache(
    scoped_refptr<DevToolsProtocol::Command> command) {
  GetContentClient()->browser()->ClearCache(host_);
  return command->SuccessResponse(NULL);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::ClearBrowserCookies(
    scoped_refptr<DevToolsProtocol::Command> command) {
  GetContentClient()->browser()->ClearCookies(host_);
  return command->SuccessResponse(NULL);
}


// Page agent handlers  -------------------------------------------------------

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageEnable(
    scoped_refptr<DevToolsProtocol::Command> command) {
  page_domain_enabled_ = true;
  // Fall through to the renderer.
  return NULL;
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageDisable(
    scoped_refptr<DevToolsProtocol::Command> command) {
  page_domain_enabled_ = false;
  OnClientDetached();
  // Fall through to the renderer.
  return NULL;
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageHandleJavaScriptDialog(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  const char* paramAccept =
      devtools::Page::handleJavaScriptDialog::kParamAccept;
  bool accept = false;
  if (!params || !params->GetBoolean(paramAccept, &accept))
    return command->InvalidParamResponse(paramAccept);
  base::string16 prompt_override;
  base::string16* prompt_override_ptr = &prompt_override;
  if (!params || !params->GetString(
      devtools::Page::handleJavaScriptDialog::kParamPromptText,
      prompt_override_ptr)) {
    prompt_override_ptr = NULL;
  }

  if (!host_)
    return command->InternalErrorResponse("Could not connect to view");

  WebContents* web_contents = WebContents::FromRenderViewHost(host_);
  if (web_contents) {
    JavaScriptDialogManager* manager =
        web_contents->GetDelegate()->GetJavaScriptDialogManager();
    if (manager && manager->HandleJavaScriptDialog(
            web_contents, accept, prompt_override_ptr)) {
      return command->SuccessResponse(new base::DictionaryValue());
    }
  }
  return command->InternalErrorResponse("No JavaScript dialog to handle");
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageNavigate(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  std::string url;
  const char* param = devtools::Page::navigate::kParamUrl;
  if (!params || !params->GetString(param, &url))
    return command->InvalidParamResponse(param);

  GURL gurl(url);
  if (!gurl.is_valid())
    return command->InternalErrorResponse("Cannot navigate to invalid URL");

  if (!host_)
    return command->InternalErrorResponse("Could not connect to view");

  WebContents* web_contents = WebContents::FromRenderViewHost(host_);
  if (web_contents) {
    web_contents->GetController()
        .LoadURL(gurl, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
    // Fall through into the renderer.
    return NULL;
  }

  return command->InternalErrorResponse("No WebContents to navigate");
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageReload(
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (!host_)
    return command->InternalErrorResponse("Could not connect to view");

  WebContents* web_contents = WebContents::FromRenderViewHost(host_);
  if (web_contents) {
    // Override only if it is crashed.
    if (!web_contents->IsCrashed())
      return NULL;

    web_contents->GetController().Reload(false);
    return command->SuccessResponse(NULL);
  }
  return command->InternalErrorResponse("No WebContents to reload");
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageGetNavigationHistory(
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (!host_)
    return command->InternalErrorResponse("Could not connect to view");
  WebContents* web_contents = WebContents::FromRenderViewHost(host_);
  if (web_contents) {
    base::DictionaryValue* result = new base::DictionaryValue();
    NavigationController& controller = web_contents->GetController();
    result->SetInteger(
        devtools::Page::getNavigationHistory::kResponseCurrentIndex,
        controller.GetCurrentEntryIndex());
    base::ListValue* entries = new base::ListValue();
    for (int i = 0; i != controller.GetEntryCount(); ++i) {
      const NavigationEntry* entry = controller.GetEntryAtIndex(i);
      base::DictionaryValue* entry_value = new base::DictionaryValue();
      entry_value->SetInteger(
          devtools::Page::NavigationEntry::kParamId,
          entry->GetUniqueID());
      entry_value->SetString(
          devtools::Page::NavigationEntry::kParamUrl,
          entry->GetURL().spec());
      entry_value->SetString(
          devtools::Page::NavigationEntry::kParamTitle,
          entry->GetTitle());
      entries->Append(entry_value);
    }
    result->Set(
        devtools::Page::getNavigationHistory::kResponseEntries,
        entries);
    return command->SuccessResponse(result);
  }
  return command->InternalErrorResponse("No WebContents to navigate");
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageNavigateToHistoryEntry(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  const char* param = devtools::Page::navigateToHistoryEntry::kParamEntryId;
  int entry_id = 0;
  if (!params || !params->GetInteger(param, &entry_id)) {
    return command->InvalidParamResponse(param);
  }

  if (!host_)
    return command->InternalErrorResponse("Could not connect to view");

  WebContents* web_contents = WebContents::FromRenderViewHost(host_);
  if (web_contents) {
    NavigationController& controller = web_contents->GetController();
    for (int i = 0; i != controller.GetEntryCount(); ++i) {
      if (controller.GetEntryAtIndex(i)->GetUniqueID() == entry_id) {
        controller.GoToIndex(i);
        return command->SuccessResponse(new base::DictionaryValue());
      }
    }
    return command->InvalidParamResponse(param);
  }
  return command->InternalErrorResponse("No WebContents to navigate");
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageCaptureScreenshot(
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (!host_ || !host_->GetView())
    return command->InternalErrorResponse("Could not connect to view");

  host_->GetSnapshotFromBrowser(
      base::Bind(&RendererOverridesHandler::ScreenshotCaptured,
          weak_factory_.GetWeakPtr(), command));
  return command->AsyncResponsePromise();
}

void RendererOverridesHandler::ScreenshotCaptured(
    scoped_refptr<DevToolsProtocol::Command> command,
    const unsigned char* png_data,
    size_t png_size) {
  if (!png_data || !png_size) {
    SendAsyncResponse(
        command->InternalErrorResponse("Unable to capture screenshot"));
    return;
  }

  std::string base_64_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(png_data), png_size),
      &base_64_data);

  base::DictionaryValue* response = new base::DictionaryValue();
  response->SetString(devtools::Page::screencastFrame::kParamData,
                      base_64_data);

  SendAsyncResponse(command->SuccessResponse(response));
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageSetTouchEmulationEnabled(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  bool enabled = false;
  if (!params || !params->GetBoolean(
      devtools::Page::setTouchEmulationEnabled::kParamEnabled,
      &enabled)) {
    // Pass to renderer.
    return NULL;
  }

  touch_emulation_enabled_ = enabled;
  UpdateTouchEventEmulationState();

  // Pass to renderer.
  return NULL;
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageCanEmulate(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* result = new base::DictionaryValue();
#if defined(OS_ANDROID)
  result->SetBoolean(devtools::kResult, false);
#else
  if (WebContents* web_contents = WebContents::FromRenderViewHost(host_)) {
    result->SetBoolean(
        devtools::kResult,
        !web_contents->GetVisibleURL().SchemeIs(kChromeDevToolsScheme));
  } else {
    result->SetBoolean(devtools::kResult, true);
  }
#endif  // defined(OS_ANDROID)
  return command->SuccessResponse(result);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageCanScreencast(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* result = new base::DictionaryValue();
#if defined(OS_ANDROID)
  result->SetBoolean(devtools::kResult, true);
#else
  result->SetBoolean(devtools::kResult, false);
#endif  // defined(OS_ANDROID)
  return command->SuccessResponse(result);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageStartScreencast(
    scoped_refptr<DevToolsProtocol::Command> command) {
  screencast_command_ = command;
  UpdateTouchEventEmulationState();
  if (!host_)
    return command->InternalErrorResponse("Could not connect to view");
  bool visible = !host_->is_hidden();
  NotifyScreencastVisibility(visible);
  if (visible) {
    if (has_last_compositor_frame_metadata_)
      InnerSwapCompositorFrame();
    else
      host_->Send(new ViewMsg_ForceRedraw(host_->GetRoutingID(), 0));
  }
  // Pass through to the renderer.
  return NULL;
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageStopScreencast(
    scoped_refptr<DevToolsProtocol::Command> command) {
  last_frame_time_ = base::TimeTicks();
  screencast_command_ = NULL;
  UpdateTouchEventEmulationState();
  // Pass through to the renderer.
  return NULL;
}

void RendererOverridesHandler::ScreencastFrameCaptured(
    const std::string& format,
    int quality,
    const cc::CompositorFrameMetadata& metadata,
    bool success,
    const SkBitmap& bitmap) {
  if (!success) {
    if (capture_retry_count_) {
      --capture_retry_count_;
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&RendererOverridesHandler::InnerSwapCompositorFrame,
                     weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kFrameRateThresholdMs));
    }
    return;
  }

  std::vector<unsigned char> data;
  SkAutoLockPixels lock_image(bitmap);
  bool encoded;
  if (format == kPng) {
    encoded = gfx::PNGCodec::Encode(
        reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
        gfx::PNGCodec::FORMAT_SkBitmap,
        gfx::Size(bitmap.width(), bitmap.height()),
        bitmap.width() * bitmap.bytesPerPixel(),
        false, std::vector<gfx::PNGCodec::Comment>(), &data);
  } else if (format == kJpeg) {
    encoded = gfx::JPEGCodec::Encode(
        reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
        gfx::JPEGCodec::FORMAT_SkBitmap,
        bitmap.width(),
        bitmap.height(),
        bitmap.width() * bitmap.bytesPerPixel(),
        quality, &data);
  } else {
    encoded = false;
  }

  if (!encoded)
    return;

  std::string base_64_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<char*>(&data[0]), data.size()),
      &base_64_data);

  base::DictionaryValue* response = new base::DictionaryValue();
  response->SetString(devtools::Page::screencastFrame::kParamData,
                      base_64_data);

  // Consider metadata empty in case it has no device scale factor.
  if (metadata.device_scale_factor != 0) {
    base::DictionaryValue* response_metadata = new base::DictionaryValue();

    RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
        host_->GetView());
    if (!view)
      return;

    gfx::SizeF viewport_size_dip = gfx::ScaleSize(
        metadata.scrollable_viewport_size, metadata.page_scale_factor);
    gfx::SizeF screen_size_dip = gfx::ScaleSize(
        view->GetPhysicalBackingSize(), 1 / metadata.device_scale_factor);

    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamDeviceScaleFactor,
        metadata.device_scale_factor);
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamPageScaleFactor,
        metadata.page_scale_factor);
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamPageScaleFactorMin,
        metadata.min_page_scale_factor);
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamPageScaleFactorMax,
        metadata.max_page_scale_factor);
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamOffsetTop,
        metadata.location_bar_content_translation.y());
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamOffsetBottom,
        screen_size_dip.height() -
            metadata.location_bar_content_translation.y() -
            viewport_size_dip.height());

    base::DictionaryValue* viewport = new base::DictionaryValue();
    viewport->SetDouble(devtools::DOM::Rect::kParamX,
                        metadata.root_scroll_offset.x());
    viewport->SetDouble(devtools::DOM::Rect::kParamY,
                        metadata.root_scroll_offset.y());
    viewport->SetDouble(devtools::DOM::Rect::kParamWidth,
                        metadata.scrollable_viewport_size.width());
    viewport->SetDouble(devtools::DOM::Rect::kParamHeight,
                        metadata.scrollable_viewport_size.height());
    response_metadata->Set(
        devtools::Page::ScreencastFrameMetadata::kParamViewport, viewport);

    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamDeviceWidth,
        screen_size_dip.width());
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamDeviceHeight,
        screen_size_dip.height());
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamScrollOffsetX,
        metadata.root_scroll_offset.x());
    response_metadata->SetDouble(
        devtools::Page::ScreencastFrameMetadata::kParamScrollOffsetY,
        metadata.root_scroll_offset.y());

    response->Set(devtools::Page::screencastFrame::kParamMetadata,
                  response_metadata);
  }

  SendNotification(devtools::Page::screencastFrame::kName, response);
}

// Quota and Usage ------------------------------------------

namespace {

typedef base::Callback<void(scoped_ptr<base::DictionaryValue>)>
    ResponseCallback;

void QueryUsageAndQuotaCompletedOnIOThread(
    scoped_ptr<base::DictionaryValue> quota,
    scoped_ptr<base::DictionaryValue> usage,
    ResponseCallback callback) {

  scoped_ptr<base::DictionaryValue> response_data(new base::DictionaryValue);
  response_data->Set(devtools::Page::queryUsageAndQuota::kResponseQuota,
                     quota.release());
  response_data->Set(devtools::Page::queryUsageAndQuota::kResponseUsage,
                     usage.release());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, base::Passed(&response_data)));
}

void DidGetHostUsage(
    base::ListValue* list,
    const std::string& client_id,
    const base::Closure& barrier,
    int64 value) {
  base::DictionaryValue* usage_item = new base::DictionaryValue;
  usage_item->SetString(devtools::Page::UsageItem::kParamId, client_id);
  usage_item->SetDouble(devtools::Page::UsageItem::kParamValue, value);
  list->Append(usage_item);
  barrier.Run();
}

void DidGetQuotaValue(base::DictionaryValue* dictionary,
                      const std::string& item_name,
                      const base::Closure& barrier,
                      storage::QuotaStatusCode status,
                      int64 value) {
  if (status == storage::kQuotaStatusOk)
    dictionary->SetDouble(item_name, value);
  barrier.Run();
}

void DidGetUsageAndQuotaForWebApps(base::DictionaryValue* quota,
                                   const std::string& item_name,
                                   const base::Closure& barrier,
                                   storage::QuotaStatusCode status,
                                   int64 used_bytes,
                                   int64 quota_in_bytes) {
  if (status == storage::kQuotaStatusOk)
    quota->SetDouble(item_name, quota_in_bytes);
  barrier.Run();
}

std::string GetStorageTypeName(storage::StorageType type) {
  switch (type) {
    case storage::kStorageTypeTemporary:
      return devtools::Page::Usage::kParamTemporary;
    case storage::kStorageTypePersistent:
      return devtools::Page::Usage::kParamPersistent;
    case storage::kStorageTypeSyncable:
      return devtools::Page::Usage::kParamSyncable;
    case storage::kStorageTypeQuotaNotManaged:
    case storage::kStorageTypeUnknown:
      NOTREACHED();
  }
  return "";
}

std::string GetQuotaClientName(storage::QuotaClient::ID id) {
  switch (id) {
    case storage::QuotaClient::kFileSystem:
      return devtools::Page::UsageItem::Id::kEnumFilesystem;
    case storage::QuotaClient::kDatabase:
      return devtools::Page::UsageItem::Id::kEnumDatabase;
    case storage::QuotaClient::kAppcache:
      return devtools::Page::UsageItem::Id::kEnumAppcache;
    case storage::QuotaClient::kIndexedDatabase:
      return devtools::Page::UsageItem::Id::kEnumIndexeddatabase;
    default:
      NOTREACHED();
  }
  return "";
}

void QueryUsageAndQuotaOnIOThread(
    scoped_refptr<storage::QuotaManager> quota_manager,
    const GURL& security_origin,
    const ResponseCallback& callback) {
  scoped_ptr<base::DictionaryValue> quota(new base::DictionaryValue);
  scoped_ptr<base::DictionaryValue> usage(new base::DictionaryValue);

  static storage::QuotaClient::ID kQuotaClients[] = {
      storage::QuotaClient::kFileSystem, storage::QuotaClient::kDatabase,
      storage::QuotaClient::kAppcache, storage::QuotaClient::kIndexedDatabase};

  static const size_t kStorageTypeCount = storage::kStorageTypeUnknown;
  std::map<storage::StorageType, base::ListValue*> storage_type_lists;

  for (size_t i = 0; i != kStorageTypeCount; i++) {
    const storage::StorageType type = static_cast<storage::StorageType>(i);
    if (type == storage::kStorageTypeQuotaNotManaged)
      continue;
    storage_type_lists[type] = new base::ListValue;
    usage->Set(GetStorageTypeName(type), storage_type_lists[type]);
  }

  const int kExpectedResults =
      2 + arraysize(kQuotaClients) * storage_type_lists.size();
  base::DictionaryValue* quota_raw_ptr = quota.get();

  // Takes ownership on usage and quota.
  base::Closure barrier = BarrierClosure(
      kExpectedResults,
      base::Bind(&QueryUsageAndQuotaCompletedOnIOThread,
                 base::Passed(&quota),
                 base::Passed(&usage),
                 callback));
  std::string host = net::GetHostOrSpecFromURL(security_origin);

  quota_manager->GetUsageAndQuotaForWebApps(
      security_origin,
      storage::kStorageTypeTemporary,
      base::Bind(&DidGetUsageAndQuotaForWebApps,
                 quota_raw_ptr,
                 std::string(devtools::Page::Quota::kParamTemporary),
                 barrier));

  quota_manager->GetPersistentHostQuota(
      host,
      base::Bind(&DidGetQuotaValue, quota_raw_ptr,
                 std::string(devtools::Page::Quota::kParamPersistent),
                 barrier));

  for (size_t i = 0; i != arraysize(kQuotaClients); i++) {
    std::map<storage::StorageType, base::ListValue*>::const_iterator iter;
    for (iter = storage_type_lists.begin();
         iter != storage_type_lists.end(); ++iter) {
      const storage::StorageType type = (*iter).first;
      if (!quota_manager->IsTrackingHostUsage(type, kQuotaClients[i])) {
        barrier.Run();
        continue;
      }
      quota_manager->GetHostUsage(
          host, type, kQuotaClients[i],
          base::Bind(&DidGetHostUsage, (*iter).second,
                     GetQuotaClientName(kQuotaClients[i]),
                     barrier));
    }
  }
}

} // namespace

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageQueryUsageAndQuota(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  std::string security_origin;
  if (!params || !params->GetString(
      devtools::Page::queryUsageAndQuota::kParamSecurityOrigin,
      &security_origin)) {
    return command->InvalidParamResponse(
        devtools::Page::queryUsageAndQuota::kParamSecurityOrigin);
  }

  ResponseCallback callback = base::Bind(
      &RendererOverridesHandler::PageQueryUsageAndQuotaCompleted,
      weak_factory_.GetWeakPtr(),
      command);

  if (!host_)
    return command->InternalErrorResponse("Could not connect to view");

  scoped_refptr<storage::QuotaManager> quota_manager =
      host_->GetProcess()->GetStoragePartition()->GetQuotaManager();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &QueryUsageAndQuotaOnIOThread,
          quota_manager,
          GURL(security_origin),
          callback));

  return command->AsyncResponsePromise();
}

void RendererOverridesHandler::PageQueryUsageAndQuotaCompleted(
    scoped_refptr<DevToolsProtocol::Command> command,
    scoped_ptr<base::DictionaryValue> response_data) {
  SendAsyncResponse(command->SuccessResponse(response_data.release()));
}

void RendererOverridesHandler::NotifyScreencastVisibility(bool visible) {
  if (visible)
    capture_retry_count_ = kCaptureRetryLimit;
  base::DictionaryValue* params = new base::DictionaryValue();
  params->SetBoolean(
      devtools::Page::screencastVisibilityChanged::kParamVisible, visible);
  SendNotification(
      devtools::Page::screencastVisibilityChanged::kName, params);
}

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::PageSetColorPickerEnabled(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* params = command->params();
  bool color_picker_enabled = false;
  if (!params || !params->GetBoolean(
      devtools::Page::setColorPickerEnabled::kParamEnabled,
      &color_picker_enabled)) {
    return command->InvalidParamResponse(
        devtools::Page::setColorPickerEnabled::kParamEnabled);
  }

  SetColorPickerEnabled(color_picker_enabled);
  return command->SuccessResponse(NULL);
}

void RendererOverridesHandler::SetColorPickerEnabled(bool enabled) {
  if (color_picker_enabled_ == enabled)
    return;

  color_picker_enabled_ = enabled;

  if (!host_)
    return;

  if (enabled) {
    host_->AddMouseEventCallback(mouse_event_callback_);
    UpdateColorPickerFrame();
  } else {
    host_->RemoveMouseEventCallback(mouse_event_callback_);
    ResetColorPickerFrame();

    WebCursor pointer_cursor;
    WebCursor::CursorInfo cursor_info;
    cursor_info.type = blink::WebCursorInfo::TypePointer;
    pointer_cursor.InitFromCursorInfo(cursor_info);
    host_->SetCursor(pointer_cursor);
  }
}

void RendererOverridesHandler::UpdateColorPickerFrame() {
  if (!host_)
    return;
  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(host_->GetView());
  if (!view)
    return;

  gfx::Size size = view->GetViewBounds().size();
  view->CopyFromCompositingSurface(
      gfx::Rect(size), size,
      base::Bind(&RendererOverridesHandler::ColorPickerFrameUpdated,
                 weak_factory_.GetWeakPtr()),
      kN32_SkColorType);
}

void RendererOverridesHandler::ResetColorPickerFrame() {
  color_picker_frame_.reset();
  last_cursor_x_ = -1;
  last_cursor_y_ = -1;
}

void RendererOverridesHandler::ColorPickerFrameUpdated(
    bool succeeded,
    const SkBitmap& bitmap) {
  if (!color_picker_enabled_)
    return;

  if (succeeded) {
    color_picker_frame_ = bitmap;
    UpdateColorPickerCursor();
  }
}

bool RendererOverridesHandler::HandleMouseEvent(
    const blink::WebMouseEvent& event) {
  last_cursor_x_ = event.x;
  last_cursor_y_ = event.y;
  if (color_picker_frame_.drawsNothing())
    return true;

  if (event.button == blink::WebMouseEvent::ButtonLeft &&
      event.type == blink::WebInputEvent::MouseDown) {
    if (last_cursor_x_ < 0 || last_cursor_x_ >= color_picker_frame_.width() ||
        last_cursor_y_ < 0 || last_cursor_y_ >= color_picker_frame_.height()) {
      return true;
    }

    SkAutoLockPixels lock_image(color_picker_frame_);
    SkColor color = color_picker_frame_.getColor(last_cursor_x_,
                                                 last_cursor_y_);
    base::DictionaryValue* color_dict = new base::DictionaryValue();
    color_dict->SetInteger("r", SkColorGetR(color));
    color_dict->SetInteger("g", SkColorGetG(color));
    color_dict->SetInteger("b", SkColorGetB(color));
    color_dict->SetInteger("a", SkColorGetA(color));
    base::DictionaryValue* response = new base::DictionaryValue();
    response->Set(devtools::Page::colorPicked::kParamColor, color_dict);
    SendNotification(devtools::Page::colorPicked::kName, response);
  }
  UpdateColorPickerCursor();
  return true;
}

void RendererOverridesHandler::UpdateColorPickerCursor() {
  if (!host_ || color_picker_frame_.drawsNothing())
    return;

  if (last_cursor_x_ < 0 || last_cursor_x_ >= color_picker_frame_.width() ||
      last_cursor_y_ < 0 || last_cursor_y_ >= color_picker_frame_.height()) {
    return;
  }

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      host_->GetView());
  if (!view)
    return;

  // Due to platform limitations, we are using two different cursors
  // depending on the platform. Mac and Win have large cursors with two circles
  // for original spot and its magnified projection; Linux gets smaller (64 px)
  // magnified projection only with centered hotspot.
  // Mac Retina requires cursor to be > 120px in order to render smoothly.

#if defined(OS_LINUX)
  const float kCursorSize = 63;
  const float kDiameter = 63;
  const float kHotspotOffset = 32;
  const float kHotspotRadius = 0;
  const float kPixelSize = 9;
#else
  const float kCursorSize = 150;
  const float kDiameter = 110;
  const float kHotspotOffset = 25;
  const float kHotspotRadius = 5;
  const float kPixelSize = 10;
#endif

  blink::WebScreenInfo screen_info;
  view->GetScreenInfo(&screen_info);
  double device_scale_factor = screen_info.deviceScaleFactor;

  skia::RefPtr<SkCanvas> canvas = skia::AdoptRef(SkCanvas::NewRasterN32(
      kCursorSize * device_scale_factor,
      kCursorSize * device_scale_factor));
  canvas->scale(device_scale_factor, device_scale_factor);
  canvas->translate(0.5f, 0.5f);

  SkPaint paint;

  // Paint original spot with cross.
  if (kHotspotRadius) {
    paint.setStrokeWidth(1);
    paint.setAntiAlias(false);
    paint.setColor(SK_ColorDKGRAY);
    paint.setStyle(SkPaint::kStroke_Style);

    canvas->drawLine(kHotspotOffset, kHotspotOffset - 2 * kHotspotRadius,
                     kHotspotOffset, kHotspotOffset - kHotspotRadius,
                     paint);
    canvas->drawLine(kHotspotOffset, kHotspotOffset + kHotspotRadius,
                     kHotspotOffset, kHotspotOffset + 2 * kHotspotRadius,
                     paint);
    canvas->drawLine(kHotspotOffset - 2 * kHotspotRadius, kHotspotOffset,
                     kHotspotOffset - kHotspotRadius, kHotspotOffset,
                     paint);
    canvas->drawLine(kHotspotOffset + kHotspotRadius, kHotspotOffset,
                     kHotspotOffset + 2 * kHotspotRadius, kHotspotOffset,
                     paint);

    paint.setStrokeWidth(2);
    paint.setAntiAlias(true);
    canvas->drawCircle(kHotspotOffset, kHotspotOffset, kHotspotRadius, paint);
  }

  // Clip circle for magnified projection.
  float padding = (kCursorSize - kDiameter) / 2;
  SkPath clip_path;
  clip_path.addOval(SkRect::MakeXYWH(padding, padding, kDiameter, kDiameter));
  clip_path.close();
  canvas->clipPath(clip_path, SkRegion::kIntersect_Op, true);

  // Project pixels.
  int pixel_count = kDiameter / kPixelSize;
  SkRect src_rect = SkRect::MakeXYWH(last_cursor_x_ - pixel_count / 2,
                                     last_cursor_y_ - pixel_count / 2,
                                     pixel_count, pixel_count);
  SkRect dst_rect = SkRect::MakeXYWH(padding, padding, kDiameter, kDiameter);
  canvas->drawBitmapRectToRect(color_picker_frame_, &src_rect, dst_rect);

  // Paint grid.
  paint.setStrokeWidth(1);
  paint.setAntiAlias(false);
  paint.setColor(SK_ColorGRAY);
  for (int i = 0; i < pixel_count; ++i) {
    canvas->drawLine(padding + i * kPixelSize, padding,
                     padding + i * kPixelSize, kCursorSize - padding, paint);
    canvas->drawLine(padding, padding + i * kPixelSize,
                     kCursorSize - padding, padding + i * kPixelSize, paint);
  }

  // Paint central pixel in red.
  SkRect pixel = SkRect::MakeXYWH((kCursorSize - kPixelSize) / 2,
                                  (kCursorSize - kPixelSize) / 2,
                                  kPixelSize, kPixelSize);
  paint.setColor(SK_ColorRED);
  paint.setStyle(SkPaint::kStroke_Style);
  canvas->drawRect(pixel, paint);

  // Paint outline.
  paint.setStrokeWidth(2);
  paint.setColor(SK_ColorDKGRAY);
  paint.setAntiAlias(true);
  canvas->drawCircle(kCursorSize / 2, kCursorSize / 2, kDiameter / 2, paint);

  SkBitmap result;
  result.allocN32Pixels(kCursorSize * device_scale_factor,
                        kCursorSize * device_scale_factor);
  canvas->readPixels(&result, 0, 0);

  WebCursor cursor;
  WebCursor::CursorInfo cursor_info;
  cursor_info.type = blink::WebCursorInfo::TypeCustom;
  cursor_info.image_scale_factor = device_scale_factor;
  cursor_info.custom_image = result;
  cursor_info.hotspot =
      gfx::Point(kHotspotOffset * device_scale_factor,
                 kHotspotOffset * device_scale_factor);
#if defined(OS_WIN)
  cursor_info.external_handle = 0;
#endif

  cursor.InitFromCursorInfo(cursor_info);
  DCHECK(host_);
  host_->SetCursor(cursor);
}

// Input agent handlers  ------------------------------------------------------

scoped_refptr<DevToolsProtocol::Response>
RendererOverridesHandler::InputEmulateTouchFromMouseEvent(
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (!screencast_command_.get())
    return command->InternalErrorResponse("Screencast should be turned on");

  base::DictionaryValue* params = command->params();
  if (!params)
    return command->NoSuchMethodErrorResponse();

  std::string type;
  if (!params->GetString(
          devtools::Input::emulateTouchFromMouseEvent::kParamType,
          &type)) {
    return command->InvalidParamResponse(
        devtools::Input::emulateTouchFromMouseEvent::kParamType);
  }

  blink::WebMouseWheelEvent wheel_event;
  blink::WebMouseEvent mouse_event;
  blink::WebMouseEvent* event = &mouse_event;

  if (type ==
      devtools::Input::emulateTouchFromMouseEvent::Type::kEnumMousePressed) {
    event->type = WebInputEvent::MouseDown;
  } else if (type ==
      devtools::Input::emulateTouchFromMouseEvent::Type::kEnumMouseReleased) {
    event->type = WebInputEvent::MouseUp;
  } else if (type ==
      devtools::Input::emulateTouchFromMouseEvent::Type::kEnumMouseMoved) {
    event->type = WebInputEvent::MouseMove;
  } else if (type ==
      devtools::Input::emulateTouchFromMouseEvent::Type::kEnumMouseWheel) {
    double deltaX = 0;
    double deltaY = 0;
    if (!params->GetDouble(
            devtools::Input::emulateTouchFromMouseEvent::kParamDeltaX,
            &deltaX)) {
      return command->InvalidParamResponse(
          devtools::Input::emulateTouchFromMouseEvent::kParamDeltaX);
    }
    if (!params->GetDouble(
            devtools::Input::emulateTouchFromMouseEvent::kParamDeltaY,
            &deltaY)) {
      return command->InvalidParamResponse(
          devtools::Input::emulateTouchFromMouseEvent::kParamDeltaY);
    }
    wheel_event.deltaX = static_cast<float>(deltaX);
    wheel_event.deltaY = static_cast<float>(deltaY);
    event = &wheel_event;
    event->type = WebInputEvent::MouseWheel;
  } else {
    return command->InvalidParamResponse(
        devtools::Input::emulateTouchFromMouseEvent::kParamType);
  }

  int modifiers = 0;
  if (params->GetInteger(
          devtools::Input::emulateTouchFromMouseEvent::kParamModifiers,
          &modifiers)) {
    if (modifiers & 1)
      event->modifiers |= WebInputEvent::AltKey;
    if (modifiers & 2)
      event->modifiers |= WebInputEvent::ControlKey;
    if (modifiers & 4)
      event->modifiers |= WebInputEvent::MetaKey;
    if (modifiers & 8)
      event->modifiers |= WebInputEvent::ShiftKey;
  }

  params->GetDouble(
      devtools::Input::emulateTouchFromMouseEvent::kParamTimestamp,
      &event->timeStampSeconds);

  if (!params->GetInteger(devtools::Input::emulateTouchFromMouseEvent::kParamX,
                          &event->x)) {
    return command->InvalidParamResponse(
        devtools::Input::emulateTouchFromMouseEvent::kParamX);
  }

  if (!params->GetInteger(devtools::Input::emulateTouchFromMouseEvent::kParamY,
                          &event->y)) {
    return command->InvalidParamResponse(
        devtools::Input::emulateTouchFromMouseEvent::kParamY);
  }

  event->windowX = event->x;
  event->windowY = event->y;
  event->globalX = event->x;
  event->globalY = event->y;

  params->GetInteger(
      devtools::Input::emulateTouchFromMouseEvent::kParamClickCount,
      &event->clickCount);

  std::string button;
  if (!params->GetString(
          devtools::Input::emulateTouchFromMouseEvent::kParamButton,
          &button)) {
    return command->InvalidParamResponse(
        devtools::Input::emulateTouchFromMouseEvent::kParamButton);
  }

  if (button == "none") {
    event->button = WebMouseEvent::ButtonNone;
  } else if (button == "left") {
    event->button = WebMouseEvent::ButtonLeft;
    event->modifiers |= WebInputEvent::LeftButtonDown;
  } else if (button == "middle") {
    event->button = WebMouseEvent::ButtonMiddle;
    event->modifiers |= WebInputEvent::MiddleButtonDown;
  } else if (button == "right") {
    event->button = WebMouseEvent::ButtonRight;
    event->modifiers |= WebInputEvent::RightButtonDown;
  } else {
    return command->InvalidParamResponse(
        devtools::Input::emulateTouchFromMouseEvent::kParamButton);
  }

  if (!host_)
    return command->InternalErrorResponse("Could not connect to view");

  if (event->type == WebInputEvent::MouseWheel)
    host_->ForwardWheelEvent(wheel_event);
  else
    host_->ForwardMouseEvent(mouse_event);
  return command->SuccessResponse(NULL);
}

void RendererOverridesHandler::UpdateTouchEventEmulationState() {
  if (!host_)
    return;
  bool enabled = touch_emulation_enabled_ || screencast_command_.get();
  host_->SetTouchEventEmulationEnabled(enabled);
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderViewHost(host_));
  if (web_contents)
    web_contents->SetForceDisableOverscrollContent(enabled);
}

}  // namespace content
