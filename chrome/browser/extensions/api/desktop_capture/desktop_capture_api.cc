// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_api.h"

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/desktop_media_list_ash.h"
#include "chrome/browser/media/desktop_streams_registry.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/native_desktop_media_list.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/tabs.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_util.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/window_capturer.h"

namespace extensions {

namespace {

const char kInvalidSourceNameError[] = "Invalid source type specified.";
const char kEmptySourcesListError[] =
    "At least one source type must be specified.";
const char kTabCaptureNotSupportedError[] = "Tab capture is not supported yet.";
const char kNoTabIdError[] = "targetTab doesn't have id field set.";
const char kNoUrlError[] = "targetTab doesn't have URL field set.";
const char kInvalidTabIdError[] = "Invalid tab specified.";
const char kTabUrlChangedError[] = "URL for the specified tab has changed.";
const char kTabUrlNotSecure[] =
    "URL scheme for the specified tab is not secure.";

DesktopCaptureChooseDesktopMediaFunction::PickerFactory* g_picker_factory =
    NULL;

}  // namespace

// static
void DesktopCaptureChooseDesktopMediaFunction::SetPickerFactoryForTests(
    PickerFactory* factory) {
  g_picker_factory = factory;
}

DesktopCaptureChooseDesktopMediaFunction::
    DesktopCaptureChooseDesktopMediaFunction() {
}

DesktopCaptureChooseDesktopMediaFunction::
    ~DesktopCaptureChooseDesktopMediaFunction() {
  // RenderViewHost may be already destroyed.
  if (render_view_host()) {
    DesktopCaptureRequestsRegistry::GetInstance()->RemoveRequest(
        render_view_host()->GetProcess()->GetID(), request_id_);
  }
}

void DesktopCaptureChooseDesktopMediaFunction::Cancel() {
  // Keep reference to |this| to ensure the object doesn't get destroyed before
  // we return.
  scoped_refptr<DesktopCaptureChooseDesktopMediaFunction> self(this);
  if (picker_) {
    picker_.reset();
    SetResult(new base::StringValue(std::string()));
    SendResponse(true);
  }
}

bool DesktopCaptureChooseDesktopMediaFunction::RunAsync() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetSize() > 0);

  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &request_id_));
  args_->Remove(0, NULL);

  scoped_ptr<api::desktop_capture::ChooseDesktopMedia::Params> params =
      api::desktop_capture::ChooseDesktopMedia::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DesktopCaptureRequestsRegistry::GetInstance()->AddRequest(
      render_view_host()->GetProcess()->GetID(), request_id_, this);

  // |web_contents| is the WebContents for which the stream is created, and will
  // also be used to determine where to show the picker's UI.
  content::WebContents* web_contents = NULL;
  base::string16 target_name;
  if (params->target_tab) {
    if (!params->target_tab->url) {
      error_ = kNoUrlError;
      return false;
    }
    origin_ = GURL(*(params->target_tab->url)).GetOrigin();

    if (!CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAllowHttpScreenCapture) &&
        !origin_.SchemeIsSecure()) {
      error_ = kTabUrlNotSecure;
      return false;
    }
    target_name = base::UTF8ToUTF16(origin_.SchemeIsSecure() ?
        net::GetHostAndOptionalPort(origin_) : origin_.spec());

    if (!params->target_tab->id) {
      error_ = kNoTabIdError;
      return false;
    }

    if (!ExtensionTabUtil::GetTabById(*(params->target_tab->id), GetProfile(),
                                      true, NULL, NULL, &web_contents, NULL)) {
      error_ = kInvalidTabIdError;
      return false;
    }
    DCHECK(web_contents);

    if (origin_ != web_contents->GetLastCommittedURL().GetOrigin()) {
      error_ = kTabUrlChangedError;
      return false;
    }
  } else {
    origin_ = GetExtension()->url();
    target_name = base::UTF8ToUTF16(GetExtension()->name());
    web_contents = content::WebContents::FromRenderViewHost(render_view_host());
    DCHECK(web_contents);
  }

  // Register to be notified when the tab is closed.
  Observe(web_contents);

  bool show_screens = false;
  bool show_windows = false;

  for (std::vector<api::desktop_capture::DesktopCaptureSourceType>::iterator
       it = params->sources.begin(); it != params->sources.end(); ++it) {
    switch (*it) {
      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_NONE:
        error_ = kInvalidSourceNameError;
        return false;

      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_SCREEN:
        show_screens = true;
        break;

      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_WINDOW:
        show_windows = true;
        break;

      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_TAB:
        error_ = kTabCaptureNotSupportedError;
        return false;
    }
  }

  if (!show_screens && !show_windows) {
    error_ = kEmptySourcesListError;
    return false;
  }

  const gfx::NativeWindow parent_window =
      web_contents->GetTopLevelNativeWindow();
  scoped_ptr<DesktopMediaList> media_list;
  if (g_picker_factory) {
    media_list = g_picker_factory->CreateModel(
        show_screens, show_windows);
    picker_ = g_picker_factory->CreatePicker();
  } else {
#if defined(USE_ASH)
    if (chrome::IsNativeWindowInAsh(parent_window)) {
      media_list.reset(new DesktopMediaListAsh(
          (show_screens ? DesktopMediaListAsh::SCREENS : 0) |
          (show_windows ? DesktopMediaListAsh::WINDOWS : 0)));
    } else
#endif
    {
      webrtc::DesktopCaptureOptions options =
          webrtc::DesktopCaptureOptions::CreateDefault();
      options.set_disable_effects(false);
      scoped_ptr<webrtc::ScreenCapturer> screen_capturer(
          show_screens ? webrtc::ScreenCapturer::Create(options) : NULL);
      scoped_ptr<webrtc::WindowCapturer> window_capturer(
          show_windows ? webrtc::WindowCapturer::Create(options) : NULL);

      media_list.reset(new NativeDesktopMediaList(
          screen_capturer.Pass(), window_capturer.Pass()));
    }

    // DesktopMediaPicker is implemented only for Windows, OSX and
    // Aura Linux builds.
#if defined(TOOLKIT_VIEWS) || defined(OS_MACOSX)
    picker_ = DesktopMediaPicker::Create();
#else
    error_ = "Desktop Capture API is not yet implemented for this platform.";
    return false;
#endif
  }
  DesktopMediaPicker::DoneCallback callback = base::Bind(
      &DesktopCaptureChooseDesktopMediaFunction::OnPickerDialogResults, this);

  picker_->Show(web_contents,
                parent_window, parent_window,
                base::UTF8ToUTF16(GetExtension()->name()),
                target_name,
                media_list.Pass(), callback);
  return true;
}

void DesktopCaptureChooseDesktopMediaFunction::WebContentsDestroyed() {
  Cancel();
}

void DesktopCaptureChooseDesktopMediaFunction::OnPickerDialogResults(
    content::DesktopMediaID source) {
  std::string result;
  if (source.type != content::DesktopMediaID::TYPE_NONE &&
      web_contents()) {
    DesktopStreamsRegistry* registry =
        MediaCaptureDevicesDispatcher::GetInstance()->
        GetDesktopStreamsRegistry();
    // TODO(miu): Once render_frame_host() is being set, we should register the
    // exact RenderFrame requesting the stream, not the main RenderFrame.  With
    // that change, also update
    // MediaCaptureDevicesDispatcher::ProcessDesktopCaptureAccessRequest().
    // http://crbug.com/304341
    content::RenderFrameHost* const main_frame = web_contents()->GetMainFrame();
    result = registry->RegisterStream(
        main_frame->GetProcess()->GetID(),
        main_frame->GetRoutingID(),
        origin_,
        source,
        GetExtension()->name());
  }

  SetResult(new base::StringValue(result));
  SendResponse(true);
}

DesktopCaptureRequestsRegistry::RequestId::RequestId(int process_id,
                                                     int request_id)
    : process_id(process_id),
      request_id(request_id) {
}

bool DesktopCaptureRequestsRegistry::RequestId::operator<(
    const RequestId& other) const {
  if (process_id != other.process_id) {
    return process_id < other.process_id;
  } else {
    return request_id < other.request_id;
  }
}

DesktopCaptureCancelChooseDesktopMediaFunction::
    DesktopCaptureCancelChooseDesktopMediaFunction() {}

DesktopCaptureCancelChooseDesktopMediaFunction::
    ~DesktopCaptureCancelChooseDesktopMediaFunction() {}

bool DesktopCaptureCancelChooseDesktopMediaFunction::RunSync() {
  int request_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &request_id));

  DesktopCaptureRequestsRegistry::GetInstance()->CancelRequest(
      render_view_host()->GetProcess()->GetID(), request_id);
  return true;
}

DesktopCaptureRequestsRegistry::DesktopCaptureRequestsRegistry() {}
DesktopCaptureRequestsRegistry::~DesktopCaptureRequestsRegistry() {}

// static
DesktopCaptureRequestsRegistry* DesktopCaptureRequestsRegistry::GetInstance() {
  return Singleton<DesktopCaptureRequestsRegistry>::get();
}

void DesktopCaptureRequestsRegistry::AddRequest(
    int process_id,
    int request_id,
    DesktopCaptureChooseDesktopMediaFunction* handler) {
  requests_.insert(
      RequestsMap::value_type(RequestId(process_id, request_id), handler));
}

void DesktopCaptureRequestsRegistry::RemoveRequest(int process_id,
                                                   int request_id) {
  requests_.erase(RequestId(process_id, request_id));
}

void DesktopCaptureRequestsRegistry::CancelRequest(int process_id,
                                                   int request_id) {
  RequestsMap::iterator it = requests_.find(RequestId(process_id, request_id));
  if (it != requests_.end())
    it->second->Cancel();
}


}  // namespace extensions
