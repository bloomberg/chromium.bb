// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_base.h"

#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/webrtc/desktop_media_list_ash.h"
#include "chrome/browser/media/webrtc/desktop_streams_registry.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/native_desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/chromium_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/manifest.h"
#include "extensions/common/switches.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

const char kInvalidSourceNameError[] = "Invalid source type specified.";
const char kEmptySourcesListError[] =
    "At least one source type must be specified.";

DesktopCaptureChooseDesktopMediaFunctionBase::PickerFactory* g_picker_factory =
    NULL;

}  // namespace

// static
void DesktopCaptureChooseDesktopMediaFunctionBase::SetPickerFactoryForTests(
    PickerFactory* factory) {
  g_picker_factory = factory;
}

DesktopCaptureChooseDesktopMediaFunctionBase::
    DesktopCaptureChooseDesktopMediaFunctionBase() {
}

DesktopCaptureChooseDesktopMediaFunctionBase::
    ~DesktopCaptureChooseDesktopMediaFunctionBase() {
  // RenderViewHost may be already destroyed.
  if (render_frame_host()) {
    DesktopCaptureRequestsRegistry::GetInstance()->RemoveRequest(
        render_frame_host()->GetProcess()->GetID(), request_id_);
  }
}

void DesktopCaptureChooseDesktopMediaFunctionBase::Cancel() {
  // Keep reference to |this| to ensure the object doesn't get destroyed before
  // we return.
  scoped_refptr<DesktopCaptureChooseDesktopMediaFunctionBase> self(this);
  if (picker_) {
    picker_.reset();
    SetResult(base::MakeUnique<base::Value>(std::string()));
    SendResponse(true);
  }
}

bool DesktopCaptureChooseDesktopMediaFunctionBase::Execute(
    const std::vector<api::desktop_capture::DesktopCaptureSourceType>& sources,
    content::WebContents* web_contents,
    const GURL& origin,
    const base::string16 target_name) {
  // Register to be notified when the tab is closed.
  Observe(web_contents);

  bool show_screens = false;
  bool show_windows = false;
  bool show_tabs = false;
  bool request_audio = false;

  for (auto source_type : sources) {
    switch (source_type) {
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
        if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            extensions::switches::kDisableTabForDesktopShare)) {
          show_tabs = true;
        }
        break;

      case api::desktop_capture::DESKTOP_CAPTURE_SOURCE_TYPE_AUDIO:
        bool has_flag = base::CommandLine::ForCurrentProcess()->HasSwitch(
            extensions::switches::kDisableDesktopCaptureAudio);
        request_audio = !has_flag;
        break;
    }
  }

  if (!show_screens && !show_windows && !show_tabs) {
    error_ = kEmptySourcesListError;
    return false;
  }

  gfx::NativeWindow parent_window = web_contents->GetTopLevelNativeWindow();
  // In case of coming from background extension page, |parent_window| will
  // be null. We are going to make the picker modal to the current browser
  // window.
  if (!parent_window) {
    Browser* target_browser = chrome::FindLastActiveWithProfile(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()));

    if (target_browser)
      parent_window = target_browser->window()->GetNativeWindow();
  }
  std::unique_ptr<DesktopMediaList> screen_list;
  std::unique_ptr<DesktopMediaList> window_list;
  std::unique_ptr<DesktopMediaList> tab_list;
  if (g_picker_factory) {
    PickerFactory::MediaListArray media_lists =
        g_picker_factory->CreateModel(show_screens, show_windows, show_tabs,
                                      request_audio);
    screen_list = std::move(media_lists[0]);
    window_list = std::move(media_lists[1]);
    tab_list = std::move(media_lists[2]);
    picker_ = g_picker_factory->CreatePicker();
  } else {
    webrtc::DesktopCaptureOptions capture_options =
        webrtc::DesktopCaptureOptions::CreateDefault();
    capture_options.set_disable_effects(false);

    // Create a screens list.
    if (show_screens) {
#if defined(USE_ASH)
      screen_list = base::MakeUnique<DesktopMediaListAsh>(
          content::DesktopMediaID::TYPE_SCREEN);
#else   // !defined(USE_ASH)
      screen_list = base::MakeUnique<NativeDesktopMediaList>(
          content::DesktopMediaID::TYPE_SCREEN,
          webrtc::DesktopCapturer::CreateScreenCapturer(capture_options));
#endif  // !defined(USE_ASH)
    }

    // Create a windows list.
    if (show_windows) {
#if defined(USE_ASH)
      window_list = base::MakeUnique<DesktopMediaListAsh>(
          content::DesktopMediaID::TYPE_WINDOW);
#else   // !defined(USE_ASH)
      window_list = base::MakeUnique<NativeDesktopMediaList>(
          content::DesktopMediaID::TYPE_WINDOW,
          webrtc::DesktopCapturer::CreateWindowCapturer(capture_options));
#endif  // !defined(USE_ASH)
    }

    if (show_tabs)
      tab_list = base::MakeUnique<TabDesktopMediaList>();

    DCHECK(screen_list || window_list || tab_list);

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
      &DesktopCaptureChooseDesktopMediaFunctionBase::OnPickerDialogResults,
      this);

  picker_->Show(web_contents, parent_window, parent_window,
                base::UTF8ToUTF16(GetCallerDisplayName()), target_name,
                std::move(screen_list), std::move(window_list),
                std::move(tab_list), request_audio, callback);
  origin_ = origin;
  return true;
}

std::string DesktopCaptureChooseDesktopMediaFunctionBase::GetCallerDisplayName()
    const {
  if (extension()->location() == Manifest::COMPONENT ||
      extension()->location() == Manifest::EXTERNAL_COMPONENT) {
    return l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME);
  } else {
    return extension()->name();
  }
}

void DesktopCaptureChooseDesktopMediaFunctionBase::WebContentsDestroyed() {
  Cancel();
}

void DesktopCaptureChooseDesktopMediaFunctionBase::OnPickerDialogResults(
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
    result = registry->RegisterStream(main_frame->GetProcess()->GetID(),
                                      main_frame->GetRoutingID(),
                                      origin_,
                                      source,
                                      extension()->name());
  }

  api::desktop_capture::ChooseDesktopMedia::Results::Options options;
  options.can_request_audio_track = source.audio_share;
  results_ = api::desktop_capture::ChooseDesktopMedia::Results::Create(result,
                                                                       options);
  SendResponse(true);
}

DesktopCaptureRequestsRegistry::RequestId::RequestId(int process_id,
                                                     int request_id)
    : process_id(process_id),
      request_id(request_id) {
}

bool DesktopCaptureRequestsRegistry::RequestId::operator<(
    const RequestId& other) const {
  return std::tie(process_id, request_id) <
         std::tie(other.process_id, other.request_id);
}

DesktopCaptureCancelChooseDesktopMediaFunctionBase::
    DesktopCaptureCancelChooseDesktopMediaFunctionBase() {}

DesktopCaptureCancelChooseDesktopMediaFunctionBase::
    ~DesktopCaptureCancelChooseDesktopMediaFunctionBase() {}

ExtensionFunction::ResponseAction
DesktopCaptureCancelChooseDesktopMediaFunctionBase::Run() {
  int request_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &request_id));

  DesktopCaptureRequestsRegistry::GetInstance()->CancelRequest(
      render_frame_host()->GetProcess()->GetID(), request_id);
  return RespondNow(NoArguments());
}

DesktopCaptureRequestsRegistry::DesktopCaptureRequestsRegistry() {}
DesktopCaptureRequestsRegistry::~DesktopCaptureRequestsRegistry() {}

// static
DesktopCaptureRequestsRegistry* DesktopCaptureRequestsRegistry::GetInstance() {
  return base::Singleton<DesktopCaptureRequestsRegistry>::get();
}

void DesktopCaptureRequestsRegistry::AddRequest(
    int process_id,
    int request_id,
    DesktopCaptureChooseDesktopMediaFunctionBase* handler) {
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
