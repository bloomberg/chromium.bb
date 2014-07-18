// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/webkit_test_controller.h"

#include <iostream>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_devtools_frontend.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/webkit_test_helpers.h"
#include "ui/gfx/codec/png_codec.h"

namespace content {

const int kTestSVGWindowWidthDip = 480;
const int kTestSVGWindowHeightDip = 360;

// WebKitTestResultPrinter ----------------------------------------------------

WebKitTestResultPrinter::WebKitTestResultPrinter(
    std::ostream* output, std::ostream* error)
    : state_(DURING_TEST),
      capture_text_only_(false),
      encode_binary_data_(false),
      output_(output),
      error_(error) {
}

WebKitTestResultPrinter::~WebKitTestResultPrinter() {
}

void WebKitTestResultPrinter::PrintTextHeader() {
  if (state_ != DURING_TEST)
    return;
  if (!capture_text_only_)
    *output_ << "Content-Type: text/plain\n";
  state_ = IN_TEXT_BLOCK;
}

void WebKitTestResultPrinter::PrintTextBlock(const std::string& block) {
  if (state_ != IN_TEXT_BLOCK)
    return;
  *output_ << block;
}

void WebKitTestResultPrinter::PrintTextFooter() {
  if (state_ != IN_TEXT_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    output_->flush();
  }
  state_ = IN_IMAGE_BLOCK;
}

void WebKitTestResultPrinter::PrintImageHeader(
    const std::string& actual_hash,
    const std::string& expected_hash) {
  if (state_ != IN_IMAGE_BLOCK || capture_text_only_)
    return;
  *output_ << "\nActualHash: " << actual_hash << "\n";
  if (!expected_hash.empty())
    *output_ << "\nExpectedHash: " << expected_hash << "\n";
}

void WebKitTestResultPrinter::PrintImageBlock(
    const std::vector<unsigned char>& png_image) {
  if (state_ != IN_IMAGE_BLOCK || capture_text_only_)
    return;
  *output_ << "Content-Type: image/png\n";
  if (encode_binary_data_) {
    PrintEncodedBinaryData(png_image);
    return;
  }

  *output_ << "Content-Length: " << png_image.size() << "\n";
  output_->write(
      reinterpret_cast<const char*>(&png_image[0]), png_image.size());
}

void WebKitTestResultPrinter::PrintImageFooter() {
  if (state_ != IN_IMAGE_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    output_->flush();
  }
  state_ = AFTER_TEST;
}

void WebKitTestResultPrinter::PrintAudioHeader() {
  DCHECK_EQ(state_, DURING_TEST);
  if (!capture_text_only_)
    *output_ << "Content-Type: audio/wav\n";
  state_ = IN_AUDIO_BLOCK;
}

void WebKitTestResultPrinter::PrintAudioBlock(
    const std::vector<unsigned char>& audio_data) {
  if (state_ != IN_AUDIO_BLOCK || capture_text_only_)
    return;
  if (encode_binary_data_) {
    PrintEncodedBinaryData(audio_data);
    return;
  }

  *output_ << "Content-Length: " << audio_data.size() << "\n";
  output_->write(
      reinterpret_cast<const char*>(&audio_data[0]), audio_data.size());
}

void WebKitTestResultPrinter::PrintAudioFooter() {
  if (state_ != IN_AUDIO_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    output_->flush();
  }
  state_ = IN_IMAGE_BLOCK;
}

void WebKitTestResultPrinter::AddMessage(const std::string& message) {
  AddMessageRaw(message + "\n");
}

void WebKitTestResultPrinter::AddMessageRaw(const std::string& message) {
  if (state_ != DURING_TEST)
    return;
  *output_ << message;
}

void WebKitTestResultPrinter::AddErrorMessage(const std::string& message) {
  if (!capture_text_only_)
    *error_ << message << "\n";
  if (state_ != DURING_TEST)
    return;
  PrintTextHeader();
  *output_ << message << "\n";
  PrintTextFooter();
  PrintImageFooter();
}

void WebKitTestResultPrinter::PrintEncodedBinaryData(
    const std::vector<unsigned char>& data) {
  *output_ << "Content-Transfer-Encoding: base64\n";

  std::string data_base64;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(&data[0]), data.size()),
      &data_base64);

  *output_ << "Content-Length: " << data_base64.length() << "\n";
  output_->write(data_base64.c_str(), data_base64.length());
}

void WebKitTestResultPrinter::CloseStderr() {
  if (state_ != AFTER_TEST)
    return;
  if (!capture_text_only_) {
    *error_ << "#EOF\n";
    error_->flush();
  }
}


// WebKitTestController -------------------------------------------------------

WebKitTestController* WebKitTestController::instance_ = NULL;

// static
WebKitTestController* WebKitTestController::Get() {
  DCHECK(instance_);
  return instance_;
}

WebKitTestController::WebKitTestController()
    : main_window_(NULL),
      test_phase_(BETWEEN_TESTS),
      is_leak_detection_enabled_(CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLeakDetection)),
      crash_when_leak_found_(false) {
  CHECK(!instance_);
  instance_ = this;

  if (is_leak_detection_enabled_) {
    std::string switchValue =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnableLeakDetection);
    crash_when_leak_found_ = switchValue == switches::kCrashOnFailure;
  }

  printer_.reset(new WebKitTestResultPrinter(&std::cout, &std::cerr));
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEncodeBinary))
    printer_->set_encode_binary_data(true);
  registrar_.Add(this,
                 NOTIFICATION_RENDERER_PROCESS_CREATED,
                 NotificationService::AllSources());
  GpuDataManager::GetInstance()->AddObserver(this);
  ResetAfterLayoutTest();
}

WebKitTestController::~WebKitTestController() {
  DCHECK(CalledOnValidThread());
  CHECK(instance_ == this);
  CHECK(test_phase_ == BETWEEN_TESTS);
  GpuDataManager::GetInstance()->RemoveObserver(this);
  DiscardMainWindow();
  instance_ = NULL;
}

bool WebKitTestController::PrepareForLayoutTest(
    const GURL& test_url,
    const base::FilePath& current_working_directory,
    bool enable_pixel_dumping,
    const std::string& expected_pixel_hash) {
  DCHECK(CalledOnValidThread());
  test_phase_ = DURING_TEST;
  current_working_directory_ = current_working_directory;
  enable_pixel_dumping_ = enable_pixel_dumping;
  expected_pixel_hash_ = expected_pixel_hash;
  test_url_ = test_url;
  printer_->reset();
  ShellBrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  if (test_url.spec().find("compositing/") != std::string::npos)
    is_compositing_test_ = true;
  initial_size_ = gfx::Size(
      Shell::kDefaultTestWindowWidthDip, Shell::kDefaultTestWindowHeightDip);
  // The W3C SVG layout tests use a different size than the other layout tests.
  if (test_url.spec().find("W3C-SVG-1.1") != std::string::npos)
    initial_size_ = gfx::Size(kTestSVGWindowWidthDip, kTestSVGWindowHeightDip);
  if (!main_window_) {
    main_window_ = content::Shell::CreateNewWindow(
        browser_context,
        GURL(),
        NULL,
        MSG_ROUTING_NONE,
        initial_size_);
    WebContentsObserver::Observe(main_window_->web_contents());
    send_configuration_to_next_host_ = true;
    current_pid_ = base::kNullProcessId;
    main_window_->LoadURL(test_url);
  } else {
#if defined(OS_MACOSX)
    // Shell::SizeTo is not implemented on all platforms.
    main_window_->SizeTo(initial_size_);
#endif
    main_window_->web_contents()->GetRenderViewHost()->GetView()
        ->SetSize(initial_size_);
    main_window_->web_contents()->GetRenderViewHost()->WasResized();
    RenderViewHost* render_view_host =
        main_window_->web_contents()->GetRenderViewHost();
    WebPreferences prefs = render_view_host->GetWebkitPreferences();
    OverrideWebkitPrefs(&prefs);
    render_view_host->UpdateWebkitPreferences(prefs);
    SendTestConfiguration();

    NavigationController::LoadURLParams params(test_url);
    params.transition_type = PageTransitionFromInt(
        PAGE_TRANSITION_TYPED | PAGE_TRANSITION_FROM_ADDRESS_BAR);
    params.should_clear_history_list = true;
    main_window_->web_contents()->GetController().LoadURLWithParams(params);
    main_window_->web_contents()->Focus();
  }
  main_window_->web_contents()->GetRenderViewHost()->SetActive(true);
  main_window_->web_contents()->GetRenderViewHost()->Focus();
  return true;
}

bool WebKitTestController::ResetAfterLayoutTest() {
  DCHECK(CalledOnValidThread());
  printer_->PrintTextFooter();
  printer_->PrintImageFooter();
  printer_->CloseStderr();
  send_configuration_to_next_host_ = false;
  test_phase_ = BETWEEN_TESTS;
  is_compositing_test_ = false;
  enable_pixel_dumping_ = false;
  expected_pixel_hash_.clear();
  test_url_ = GURL();
  prefs_ = WebPreferences();
  should_override_prefs_ = false;

#if defined(OS_ANDROID)
  // Re-using the shell's main window on Android causes issues with networking
  // requests never succeeding. See http://crbug.com/277652.
  DiscardMainWindow();
#endif
  return true;
}

void WebKitTestController::SetTempPath(const base::FilePath& temp_path) {
  temp_path_ = temp_path;
}

void WebKitTestController::RendererUnresponsive() {
  DCHECK(CalledOnValidThread());
  LOG(WARNING) << "renderer unresponsive";
}

void WebKitTestController::WorkerCrashed() {
  DCHECK(CalledOnValidThread());
  printer_->AddErrorMessage("#CRASHED - worker");
  DiscardMainWindow();
}

void WebKitTestController::OverrideWebkitPrefs(WebPreferences* prefs) {
  if (should_override_prefs_) {
    *prefs = prefs_;
  } else {
    ApplyLayoutTestDefaultPreferences(prefs);
    if (is_compositing_test_) {
      CommandLine& command_line = *CommandLine::ForCurrentProcess();
      if (!command_line.HasSwitch(switches::kDisableGpu))
        prefs->accelerated_2d_canvas_enabled = true;
      prefs->accelerated_compositing_for_video_enabled = true;
      prefs->mock_scrollbars_enabled = true;
    }
  }
}

void WebKitTestController::OpenURL(const GURL& url) {
  if (test_phase_ != DURING_TEST)
    return;

  Shell::CreateNewWindow(main_window_->web_contents()->GetBrowserContext(),
                         url,
                         main_window_->web_contents()->GetSiteInstance(),
                         MSG_ROUTING_NONE,
                         gfx::Size());
}

void WebKitTestController::TestFinishedInSecondaryWindow() {
  RenderViewHost* render_view_host =
      main_window_->web_contents()->GetRenderViewHost();
  render_view_host->Send(
      new ShellViewMsg_NotifyDone(render_view_host->GetRoutingID()));
}

bool WebKitTestController::IsMainWindow(WebContents* web_contents) const {
  return main_window_ && web_contents == main_window_->web_contents();
}

bool WebKitTestController::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebKitTestController, message)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_PrintMessage, OnPrintMessage)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_TextDump, OnTextDump)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ImageDump, OnImageDump)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_AudioDump, OnAudioDump)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_OverridePreferences,
                        OnOverridePreferences)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_TestFinished, OnTestFinished)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ClearDevToolsLocalStorage,
                        OnClearDevToolsLocalStorage)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ShowDevTools, OnShowDevTools)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_CloseDevTools, OnCloseDevTools)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_GoToOffset, OnGoToOffset)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_Reload, OnReload)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_LoadURLForFrame, OnLoadURLForFrame)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_CaptureSessionHistory,
                        OnCaptureSessionHistory)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_CloseRemainingWindows,
                        OnCloseRemainingWindows)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ResetDone, OnResetDone)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_LeakDetectionDone, OnLeakDetectionDone)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebKitTestController::PluginCrashed(const base::FilePath& plugin_path,
                                         base::ProcessId plugin_pid) {
  DCHECK(CalledOnValidThread());
  printer_->AddErrorMessage(
      base::StringPrintf("#CRASHED - plugin (pid %d)", plugin_pid));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&WebKitTestController::DiscardMainWindow),
                 base::Unretained(this)));
}

void WebKitTestController::RenderViewCreated(RenderViewHost* render_view_host) {
  DCHECK(CalledOnValidThread());
  // Might be kNullProcessHandle, in which case we will receive a notification
  // later when the RenderProcessHost was created.
  if (render_view_host->GetProcess()->GetHandle() != base::kNullProcessHandle)
    current_pid_ = base::GetProcId(render_view_host->GetProcess()->GetHandle());
  if (!send_configuration_to_next_host_)
    return;
  send_configuration_to_next_host_ = false;
  SendTestConfiguration();
}

void WebKitTestController::RenderProcessGone(base::TerminationStatus status) {
  DCHECK(CalledOnValidThread());
  if (current_pid_ != base::kNullProcessId) {
    printer_->AddErrorMessage(std::string("#CRASHED - renderer (pid ") +
                              base::IntToString(current_pid_) + ")");
  } else {
    printer_->AddErrorMessage("#CRASHED - renderer");
  }
  DiscardMainWindow();
}

void WebKitTestController::DevToolsProcessCrashed() {
  DCHECK(CalledOnValidThread());
  printer_->AddErrorMessage("#CRASHED - devtools");
  DiscardMainWindow();
}

void WebKitTestController::WebContentsDestroyed() {
  DCHECK(CalledOnValidThread());
  printer_->AddErrorMessage("FAIL: main window was destroyed");
  DiscardMainWindow();
}

void WebKitTestController::Observe(int type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  DCHECK(CalledOnValidThread());
  switch (type) {
    case NOTIFICATION_RENDERER_PROCESS_CREATED: {
      if (!main_window_)
        return;
      RenderViewHost* render_view_host =
          main_window_->web_contents()->GetRenderViewHost();
      if (!render_view_host)
        return;
      RenderProcessHost* render_process_host =
          Source<RenderProcessHost>(source).ptr();
      if (render_process_host != render_view_host->GetProcess())
        return;
      current_pid_ = base::GetProcId(render_process_host->GetHandle());
      break;
    }
    default:
      NOTREACHED();
  }
}

void WebKitTestController::OnGpuProcessCrashed(
    base::TerminationStatus exit_code) {
  DCHECK(CalledOnValidThread());
  printer_->AddErrorMessage("#CRASHED - gpu");
  DiscardMainWindow();
}

void WebKitTestController::DiscardMainWindow() {
  // If we're running a test, we need to close all windows and exit the message
  // loop. Otherwise, we're already outside of the message loop, and we just
  // discard the main window.
  WebContentsObserver::Observe(NULL);
  if (test_phase_ != BETWEEN_TESTS) {
    Shell::CloseAllWindows();
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
    test_phase_ = CLEAN_UP;
  } else if (main_window_) {
    main_window_->Close();
  }
  main_window_ = NULL;
  current_pid_ = base::kNullProcessId;
}

void WebKitTestController::SendTestConfiguration() {
  RenderViewHost* render_view_host =
      main_window_->web_contents()->GetRenderViewHost();
  ShellTestConfiguration params;
  params.current_working_directory = current_working_directory_;
  params.temp_path = temp_path_;
  params.test_url = test_url_;
  params.enable_pixel_dumping = enable_pixel_dumping_;
  params.allow_external_pages = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAllowExternalPages);
  params.expected_pixel_hash = expected_pixel_hash_;
  params.initial_size = initial_size_;
  render_view_host->Send(new ShellViewMsg_SetTestConfiguration(
      render_view_host->GetRoutingID(), params));
}

void WebKitTestController::OnTestFinished() {
  test_phase_ = CLEAN_UP;
  if (!printer_->output_finished())
    printer_->PrintImageFooter();
  RenderViewHost* render_view_host =
      main_window_->web_contents()->GetRenderViewHost();
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&WebKitTestController::Send),
                 base::Unretained(this),
                 new ShellViewMsg_Reset(render_view_host->GetRoutingID())));
}

void WebKitTestController::OnImageDump(
    const std::string& actual_pixel_hash,
    const SkBitmap& image) {
  SkAutoLockPixels image_lock(image);

  printer_->PrintImageHeader(actual_pixel_hash, expected_pixel_hash_);

  // Only encode and dump the png if the hashes don't match. Encoding the
  // image is really expensive.
  if (actual_pixel_hash != expected_pixel_hash_) {
    std::vector<unsigned char> png;

    // Only the expected PNGs for Mac have a valid alpha channel.
#if defined(OS_MACOSX)
    bool discard_transparency = false;
#else
    bool discard_transparency = true;
#endif
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableOverlayFullscreenVideo))
      discard_transparency = false;

    std::vector<gfx::PNGCodec::Comment> comments;
    comments.push_back(gfx::PNGCodec::Comment("checksum", actual_pixel_hash));
    bool success = gfx::PNGCodec::Encode(
        static_cast<const unsigned char*>(image.getPixels()),
        gfx::PNGCodec::FORMAT_BGRA,
        gfx::Size(image.width(), image.height()),
        static_cast<int>(image.rowBytes()),
        discard_transparency,
        comments,
        &png);
    if (success)
      printer_->PrintImageBlock(png);
  }
  printer_->PrintImageFooter();
}

void WebKitTestController::OnAudioDump(const std::vector<unsigned char>& dump) {
  printer_->PrintAudioHeader();
  printer_->PrintAudioBlock(dump);
  printer_->PrintAudioFooter();
}

void WebKitTestController::OnTextDump(const std::string& dump) {
  printer_->PrintTextHeader();
  printer_->PrintTextBlock(dump);
  printer_->PrintTextFooter();
}

void WebKitTestController::OnPrintMessage(const std::string& message) {
  printer_->AddMessageRaw(message);
}

void WebKitTestController::OnOverridePreferences(const WebPreferences& prefs) {
  should_override_prefs_ = true;
  prefs_ = prefs;
}

void WebKitTestController::OnClearDevToolsLocalStorage() {
  ShellBrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  StoragePartition* storage_partition =
      BrowserContext::GetStoragePartition(browser_context, NULL);
  storage_partition->GetDOMStorageContext()->DeleteLocalStorage(
      content::GetDevToolsPathAsURL("", "").GetOrigin());
}

void WebKitTestController::OnShowDevTools(const std::string& settings,
                                          const std::string& frontend_url) {
  main_window_->ShowDevToolsForTest(settings, frontend_url);
}

void WebKitTestController::OnCloseDevTools() {
  main_window_->CloseDevTools();
}

void WebKitTestController::OnGoToOffset(int offset) {
  main_window_->GoBackOrForward(offset);
}

void WebKitTestController::OnReload() {
  main_window_->Reload();
}

void WebKitTestController::OnLoadURLForFrame(const GURL& url,
                                             const std::string& frame_name) {
  main_window_->LoadURLForFrame(url, frame_name);
}

void WebKitTestController::OnCaptureSessionHistory() {
  std::vector<int> routing_ids;
  std::vector<std::vector<PageState> > session_histories;
  std::vector<unsigned> current_entry_indexes;

  RenderViewHost* render_view_host =
      main_window_->web_contents()->GetRenderViewHost();

  for (std::vector<Shell*>::iterator window = Shell::windows().begin();
       window != Shell::windows().end();
       ++window) {
    WebContents* web_contents = (*window)->web_contents();
    // Only capture the history from windows in the same process as the main
    // window. During layout tests, we only use two processes when an
    // devtools window is open. This should not happen during history navigation
    // tests.
    if (render_view_host->GetProcess() !=
        web_contents->GetRenderViewHost()->GetProcess()) {
      NOTREACHED();
      continue;
    }
    routing_ids.push_back(web_contents->GetRenderViewHost()->GetRoutingID());
    current_entry_indexes.push_back(
        web_contents->GetController().GetCurrentEntryIndex());
    std::vector<PageState> history;
    for (int entry = 0; entry < web_contents->GetController().GetEntryCount();
         ++entry) {
      PageState state = web_contents->GetController().GetEntryAtIndex(entry)->
          GetPageState();
      if (!state.IsValid()) {
        state = PageState::CreateFromURL(
            web_contents->GetController().GetEntryAtIndex(entry)->GetURL());
      }
      history.push_back(state);
    }
    session_histories.push_back(history);
  }

  Send(new ShellViewMsg_SessionHistory(render_view_host->GetRoutingID(),
                                       routing_ids,
                                       session_histories,
                                       current_entry_indexes));
}

void WebKitTestController::OnCloseRemainingWindows() {
  DevToolsManager::GetInstance()->CloseAllClientHosts();
  std::vector<Shell*> open_windows(Shell::windows());
  for (size_t i = 0; i < open_windows.size(); ++i) {
    if (open_windows[i] != main_window_)
      open_windows[i]->Close();
  }
  base::MessageLoop::current()->RunUntilIdle();
}

void WebKitTestController::OnResetDone() {
  if (is_leak_detection_enabled_) {
    if (main_window_ && main_window_->web_contents()) {
      RenderViewHost* render_view_host =
          main_window_->web_contents()->GetRenderViewHost();
      render_view_host->Send(
          new ShellViewMsg_TryLeakDetection(render_view_host->GetRoutingID()));
    }
    return;
  }

  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
}

void WebKitTestController::OnLeakDetectionDone(
    const LeakDetectionResult& result) {
  if (!result.leaked) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
    return;
  }

  printer_->AddErrorMessage(
      base::StringPrintf("#LEAK - renderer pid %d (%s)", current_pid_,
                         result.detail.c_str()));
  CHECK(!crash_when_leak_found_);

  DiscardMainWindow();
}

}  // namespace content
