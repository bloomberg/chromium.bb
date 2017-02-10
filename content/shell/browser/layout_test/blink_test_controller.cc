// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/blink_test_controller.h"

#include <stddef.h>

#include <iostream>
#include <set>
#include <utility>

#include "base/base64.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/associated_interface_provider.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/shell/browser/layout_test/layout_test_bluetooth_chooser_factory.h"
#include "content/shell/browser/layout_test/layout_test_devtools_frontend.h"
#include "content/shell/browser/layout_test/layout_test_first_device_bluetooth_chooser.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_devtools_frontend.h"
#include "content/shell/common/layout_test/layout_test_messages.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/renderer/layout_test/blink_test_helpers.h"
#include "ui/gfx/codec/png_codec.h"

namespace content {

namespace {

const int kTestSVGWindowWidthDip = 480;
const int kTestSVGWindowHeightDip = 360;

}  // namespace

// BlinkTestResultPrinter ----------------------------------------------------

BlinkTestResultPrinter::BlinkTestResultPrinter(std::ostream* output,
                                               std::ostream* error)
    : state_(DURING_TEST),
      capture_text_only_(false),
      encode_binary_data_(false),
      output_(output),
      error_(error) {
}

BlinkTestResultPrinter::~BlinkTestResultPrinter() {
}

void BlinkTestResultPrinter::PrintTextHeader() {
  if (state_ != DURING_TEST)
    return;
  if (!capture_text_only_)
    *output_ << "Content-Type: text/plain\n";
  state_ = IN_TEXT_BLOCK;
}

void BlinkTestResultPrinter::PrintTextBlock(const std::string& block) {
  if (state_ != IN_TEXT_BLOCK)
    return;
  *output_ << block;
}

void BlinkTestResultPrinter::PrintTextFooter() {
  if (state_ != IN_TEXT_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    output_->flush();
  }
  state_ = IN_IMAGE_BLOCK;
}

void BlinkTestResultPrinter::PrintImageHeader(
    const std::string& actual_hash,
    const std::string& expected_hash) {
  if (state_ != IN_IMAGE_BLOCK || capture_text_only_)
    return;
  *output_ << "\nActualHash: " << actual_hash << "\n";
  if (!expected_hash.empty())
    *output_ << "\nExpectedHash: " << expected_hash << "\n";
}

void BlinkTestResultPrinter::PrintImageBlock(
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

void BlinkTestResultPrinter::PrintImageFooter() {
  if (state_ != IN_IMAGE_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    output_->flush();
  }
  state_ = AFTER_TEST;
}

void BlinkTestResultPrinter::PrintAudioHeader() {
  DCHECK_EQ(state_, DURING_TEST);
  if (!capture_text_only_)
    *output_ << "Content-Type: audio/wav\n";
  state_ = IN_AUDIO_BLOCK;
}

void BlinkTestResultPrinter::PrintAudioBlock(
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

void BlinkTestResultPrinter::PrintAudioFooter() {
  if (state_ != IN_AUDIO_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    output_->flush();
  }
  state_ = IN_IMAGE_BLOCK;
}

void BlinkTestResultPrinter::AddMessage(const std::string& message) {
  AddMessageRaw(message + "\n");
}

void BlinkTestResultPrinter::AddMessageRaw(const std::string& message) {
  if (state_ != DURING_TEST)
    return;
  *output_ << message;
}

void BlinkTestResultPrinter::AddErrorMessage(const std::string& message) {
  if (!capture_text_only_)
    *error_ << message << "\n";
  if (state_ != DURING_TEST)
    return;
  PrintTextHeader();
  *output_ << message << "\n";
  PrintTextFooter();
  PrintImageFooter();
}

void BlinkTestResultPrinter::PrintEncodedBinaryData(
    const std::vector<unsigned char>& data) {
  *output_ << "Content-Transfer-Encoding: base64\n";

  std::string data_base64;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(&data[0]), data.size()),
      &data_base64);

  *output_ << "Content-Length: " << data_base64.length() << "\n";
  output_->write(data_base64.c_str(), data_base64.length());
}

void BlinkTestResultPrinter::CloseStderr() {
  if (state_ != AFTER_TEST)
    return;
  if (!capture_text_only_) {
    *error_ << "#EOF\n";
    error_->flush();
  }
}

// BlinkTestController -------------------------------------------------------

BlinkTestController* BlinkTestController::instance_ = NULL;

// static
BlinkTestController* BlinkTestController::Get() {
  DCHECK(instance_);
  return instance_;
}

BlinkTestController::BlinkTestController()
    : main_window_(NULL),
      test_phase_(BETWEEN_TESTS),
      is_leak_detection_enabled_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableLeakDetection)),
      crash_when_leak_found_(false),
      devtools_frontend_(NULL),
      render_process_host_observer_(this) {
  CHECK(!instance_);
  instance_ = this;

  if (is_leak_detection_enabled_) {
    std::string switchValue =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnableLeakDetection);
    crash_when_leak_found_ = switchValue == switches::kCrashOnFailure;
  }

  printer_.reset(new BlinkTestResultPrinter(&std::cout, &std::cerr));
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEncodeBinary))
    printer_->set_encode_binary_data(true);
  registrar_.Add(this,
                 NOTIFICATION_RENDERER_PROCESS_CREATED,
                 NotificationService::AllSources());
  GpuDataManager::GetInstance()->AddObserver(this);
  ResetAfterLayoutTest();
}

BlinkTestController::~BlinkTestController() {
  DCHECK(CalledOnValidThread());
  CHECK(instance_ == this);
  CHECK(test_phase_ == BETWEEN_TESTS);
  GpuDataManager::GetInstance()->RemoveObserver(this);
  DiscardMainWindow();
  instance_ = NULL;
}

bool BlinkTestController::PrepareForLayoutTest(
    const GURL& test_url,
    const base::FilePath& current_working_directory,
    bool enable_pixel_dumping,
    const std::string& expected_pixel_hash) {
  DCHECK(CalledOnValidThread());
  test_phase_ = DURING_TEST;
  current_working_directory_ = current_working_directory;
  enable_pixel_dumping_ = enable_pixel_dumping;
  expected_pixel_hash_ = expected_pixel_hash;
  if (test_url.spec().find("/inspector-unit/") == std::string::npos)
    test_url_ = test_url;
  else
    test_url_ = LayoutTestDevToolsFrontend::MapJSTestURL(test_url);
  did_send_initial_test_configuration_ = false;
  printer_->reset();
  frame_to_layout_dump_map_.clear();
  render_process_host_observer_.RemoveAll();
  all_observed_render_process_hosts_.clear();
  main_window_render_process_hosts_.clear();
  accumulated_layout_test_runtime_flags_changes_.Clear();
  layout_test_control_map_.clear();
  ShellBrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  is_compositing_test_ =
      test_url_.spec().find("compositing/") != std::string::npos;
  initial_size_ = Shell::GetShellDefaultSize();
  // The W3C SVG layout tests use a different size than the other layout tests.
  if (test_url_.spec().find("W3C-SVG-1.1") != std::string::npos)
    initial_size_ = gfx::Size(kTestSVGWindowWidthDip, kTestSVGWindowHeightDip);
  if (!main_window_) {
    main_window_ = content::Shell::CreateNewWindow(
        browser_context,
        GURL(),
        NULL,
        initial_size_);
    WebContentsObserver::Observe(main_window_->web_contents());
    current_pid_ = base::kNullProcessId;
    default_prefs_ =
      main_window_->web_contents()->GetRenderViewHost()->GetWebkitPreferences();
    main_window_->LoadURL(test_url_);
  } else {
#if defined(OS_MACOSX)
    // Shell::SizeTo is not implemented on all platforms.
    main_window_->SizeTo(initial_size_);
#endif
    main_window_->web_contents()
        ->GetRenderViewHost()
        ->GetWidget()
        ->GetView()
        ->SetSize(initial_size_);
    main_window_->web_contents()
        ->GetRenderViewHost()
        ->GetWidget()
        ->WasResized();
    RenderViewHost* render_view_host =
        main_window_->web_contents()->GetRenderViewHost();

    // Compositing tests override the default preferences (see
    // BlinkTestController::OverrideWebkitPrefs) so we force them to be
    // calculated again to ensure is_compositing_test_ changes are picked up.
    OverrideWebkitPrefs(&default_prefs_);

    render_view_host->UpdateWebkitPreferences(default_prefs_);
    HandleNewRenderFrameHost(render_view_host->GetMainFrame());

    NavigationController::LoadURLParams params(test_url_);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    params.should_clear_history_list = true;
    main_window_->web_contents()->GetController().LoadURLWithParams(params);
    main_window_->web_contents()->Focus();
  }
  main_window_->web_contents()->GetRenderViewHost()->GetWidget()->SetActive(
      true);
  main_window_->web_contents()->GetRenderViewHost()->GetWidget()->Focus();
  return true;
}

bool BlinkTestController::ResetAfterLayoutTest() {
  DCHECK(CalledOnValidThread());
  printer_->PrintTextFooter();
  printer_->PrintImageFooter();
  printer_->CloseStderr();
  did_send_initial_test_configuration_ = false;
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

void BlinkTestController::SetTempPath(const base::FilePath& temp_path) {
  temp_path_ = temp_path;
}

void BlinkTestController::RendererUnresponsive() {
  DCHECK(CalledOnValidThread());
  LOG(WARNING) << "renderer unresponsive";
}

void BlinkTestController::OverrideWebkitPrefs(WebPreferences* prefs) {
  if (should_override_prefs_) {
    *prefs = prefs_;
  } else {
    ApplyLayoutTestDefaultPreferences(prefs);
    if (is_compositing_test_) {
      base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
      if (!command_line.HasSwitch(switches::kDisableGpu))
        prefs->accelerated_2d_canvas_enabled = true;
      prefs->mock_scrollbars_enabled = true;
    }
  }
}

void BlinkTestController::OpenURL(const GURL& url) {
  if (test_phase_ != DURING_TEST)
    return;

  Shell::CreateNewWindow(main_window_->web_contents()->GetBrowserContext(),
                         url,
                         main_window_->web_contents()->GetSiteInstance(),
                         gfx::Size());
}

void BlinkTestController::OnTestFinishedInSecondaryRenderer() {
  RenderViewHost* main_render_view_host =
      main_window_->web_contents()->GetRenderViewHost();
  main_render_view_host->Send(new ShellViewMsg_TestFinishedInSecondaryRenderer(
      main_render_view_host->GetRoutingID()));
}

bool BlinkTestController::IsMainWindow(WebContents* web_contents) const {
  return main_window_ && web_contents == main_window_->web_contents();
}

std::unique_ptr<BluetoothChooser> BlinkTestController::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  if (bluetooth_chooser_factory_) {
    return bluetooth_chooser_factory_->RunBluetoothChooser(frame,
                                                           event_handler);
  }
  return base::MakeUnique<LayoutTestFirstDeviceBluetoothChooser>(event_handler);
}

bool BlinkTestController::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BlinkTestController, message)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_PrintMessage, OnPrintMessage)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_TextDump, OnTextDump)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_InitiateLayoutDump,
                        OnInitiateLayoutDump)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ImageDump, OnImageDump)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_AudioDump, OnAudioDump)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_OverridePreferences,
                        OnOverridePreferences)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_TestFinished, OnTestFinished)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ClearDevToolsLocalStorage,
                        OnClearDevToolsLocalStorage)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ShowDevTools, OnShowDevTools)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_EvaluateInDevTools,
                        OnEvaluateInDevTools)
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
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_SetBluetoothManualChooser,
                        OnSetBluetoothManualChooser)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_GetBluetoothManualChooserEvents,
                        OnGetBluetoothManualChooserEvents)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_SendBluetoothManualChooserEvent,
                        OnSendBluetoothManualChooserEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool BlinkTestController::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(BlinkTestController, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_LayoutDumpResponse,
                        OnLayoutDumpResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BlinkTestController::PluginCrashed(const base::FilePath& plugin_path,
                                        base::ProcessId plugin_pid) {
  DCHECK(CalledOnValidThread());
  printer_->AddErrorMessage(
      base::StringPrintf("#CRASHED - plugin (pid %d)", plugin_pid));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&BlinkTestController::DiscardMainWindow),
                 base::Unretained(this)));
}

void BlinkTestController::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  DCHECK(CalledOnValidThread());
  HandleNewRenderFrameHost(render_frame_host);
}

void BlinkTestController::DevToolsProcessCrashed() {
  DCHECK(CalledOnValidThread());
  printer_->AddErrorMessage("#CRASHED - devtools");
  if (devtools_frontend_)
      devtools_frontend_->Close();
  devtools_frontend_ = NULL;
}

void BlinkTestController::WebContentsDestroyed() {
  DCHECK(CalledOnValidThread());
  printer_->AddErrorMessage("FAIL: main window was destroyed");
  DiscardMainWindow();
}

void BlinkTestController::RenderProcessHostDestroyed(
    RenderProcessHost* render_process_host) {
  render_process_host_observer_.Remove(render_process_host);
  all_observed_render_process_hosts_.erase(render_process_host);
  main_window_render_process_hosts_.erase(render_process_host);
}

void BlinkTestController::RenderProcessExited(
    RenderProcessHost* render_process_host,
    base::TerminationStatus status,
    int exit_code) {
  DCHECK(CalledOnValidThread());
  switch (status) {
    case base::TerminationStatus::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TerminationStatus::TERMINATION_STATUS_STILL_RUNNING:
      break;

    case base::TerminationStatus::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TerminationStatus::TERMINATION_STATUS_LAUNCH_FAILED:
    case base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TerminationStatus::TERMINATION_STATUS_PROCESS_WAS_KILLED:
    default: {
      base::ProcessHandle handle = render_process_host->GetHandle();
      if (handle != base::kNullProcessHandle) {
        printer_->AddErrorMessage(std::string("#CRASHED - renderer (pid ") +
                                  base::IntToString(base::GetProcId(handle)) +
                                  ")");
      } else {
        printer_->AddErrorMessage("#CRASHED - renderer");
      }

      DiscardMainWindow();
      break;
    }
  }
}

void BlinkTestController::Observe(int type,
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

void BlinkTestController::OnGpuProcessCrashed(
    base::TerminationStatus exit_code) {
  DCHECK(CalledOnValidThread());
  printer_->AddErrorMessage("#CRASHED - gpu");
  DiscardMainWindow();
}

void BlinkTestController::DiscardMainWindow() {
  // If we're running a test, we need to close all windows and exit the message
  // loop. Otherwise, we're already outside of the message loop, and we just
  // discard the main window.
  WebContentsObserver::Observe(NULL);
  if (test_phase_ != BETWEEN_TESTS) {
    Shell::CloseAllWindows();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    test_phase_ = CLEAN_UP;
  } else if (main_window_) {
    main_window_->Close();
  }
  main_window_ = NULL;
  current_pid_ = base::kNullProcessId;
}

void BlinkTestController::HandleNewRenderFrameHost(RenderFrameHost* frame) {
  // All RenderFrameHosts in layout tests should get Mojo bindings.
  if (!(frame->GetEnabledBindings() & BINDINGS_POLICY_MOJO))
    frame->AllowBindings(BINDINGS_POLICY_MOJO);

  RenderProcessHost* process = frame->GetProcess();
  bool main_window =
      WebContents::FromRenderFrameHost(frame) == main_window_->web_contents();

  // Track pid of the renderer handling the main frame.
  if (main_window && frame->GetParent() == nullptr) {
    base::ProcessHandle process_handle = process->GetHandle();
    if (process_handle != base::kNullProcessHandle)
      current_pid_ = base::GetProcId(process_handle);
  }

  // Is this the 1st time this renderer contains parts of the main test window?
  if (main_window &&
      !base::ContainsKey(main_window_render_process_hosts_, process)) {
    main_window_render_process_hosts_.insert(process);

    // Make sure the new renderer process has a test configuration shared with
    // other renderers.
    mojom::ShellTestConfigurationPtr params =
        mojom::ShellTestConfiguration::New();
    params->allow_external_pages = false;
    params->current_working_directory = current_working_directory_;
    params->temp_path = temp_path_;
    params->test_url = test_url_;
    params->enable_pixel_dumping = enable_pixel_dumping_;
    params->allow_external_pages =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAllowExternalPages);
    params->expected_pixel_hash = expected_pixel_hash_;
    params->initial_size = initial_size_;

    if (did_send_initial_test_configuration_) {
      GetLayoutTestControlPtr(frame)->ReplicateTestConfiguration(
          std::move(params));
    } else {
      did_send_initial_test_configuration_ = true;
      GetLayoutTestControlPtr(frame)->SetTestConfiguration(std::move(params));
    }
  }

  // Is this a previously unknown renderer process?
  if (!render_process_host_observer_.IsObserving(process)) {
    render_process_host_observer_.Add(process);
    all_observed_render_process_hosts_.insert(process);

    if (!main_window) {
      GetLayoutTestControlPtr(frame)->SetupSecondaryRenderer();
    }

    process->Send(new LayoutTestMsg_ReplicateLayoutTestRuntimeFlagsChanges(
        accumulated_layout_test_runtime_flags_changes_));
  }
}

void BlinkTestController::OnTestFinished() {
  test_phase_ = CLEAN_UP;
  if (!printer_->output_finished())
    printer_->PrintImageFooter();
  RenderViewHost* render_view_host =
      main_window_->web_contents()->GetRenderViewHost();
  main_window_->web_contents()->ExitFullscreen(/*will_cause_resize=*/false);

  ShellBrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  StoragePartition* storage_partition =
      BrowserContext::GetStoragePartition(browser_context, nullptr);
  storage_partition->GetServiceWorkerContext()->ClearAllServiceWorkersForTest(
      base::Bind(base::IgnoreResult(&BlinkTestController::Send),
                 base::Unretained(this),
                 new ShellViewMsg_Reset(render_view_host->GetRoutingID())));
  storage_partition->ClearBluetoothAllowedDevicesMapForTesting();
}

void BlinkTestController::OnImageDump(const std::string& actual_pixel_hash,
                                      const SkBitmap& image) {
  SkAutoLockPixels image_lock(image);

  printer_->PrintImageHeader(actual_pixel_hash, expected_pixel_hash_);

  // Only encode and dump the png if the hashes don't match. Encoding the
  // image is really expensive.
  if (actual_pixel_hash != expected_pixel_hash_) {
    std::vector<unsigned char> png;

    bool discard_transparency = true;
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kForceOverlayFullscreenVideo))
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

void BlinkTestController::OnAudioDump(const std::vector<unsigned char>& dump) {
  printer_->PrintAudioHeader();
  printer_->PrintAudioBlock(dump);
  printer_->PrintAudioFooter();
}

void BlinkTestController::OnTextDump(const std::string& dump) {
  printer_->PrintTextHeader();
  printer_->PrintTextBlock(dump);
  printer_->PrintTextFooter();
}

void BlinkTestController::OnInitiateLayoutDump() {
  int number_of_messages = 0;
  for (RenderFrameHost* rfh : main_window_->web_contents()->GetAllFrames()) {
    if (!rfh->IsRenderFrameLive())
      continue;

    ++number_of_messages;
    GetLayoutTestControlPtr(rfh)->LayoutDumpRequest();
  }

  pending_layout_dumps_ = number_of_messages;
}

void BlinkTestController::OnLayoutTestRuntimeFlagsChanged(
    int sender_process_host_id,
    const base::DictionaryValue& changed_layout_test_runtime_flags) {
  // Stash the accumulated changes for future, not-yet-created renderers.
  accumulated_layout_test_runtime_flags_changes_.MergeDictionary(
      &changed_layout_test_runtime_flags);

  // Propagate the changes to all the tracked renderer processes.
  for (RenderProcessHost* process : all_observed_render_process_hosts_) {
    // Do not propagate the changes back to the process that originated them.
    // (propagating them back could also clobber subsequent changes in the
    // originator).
    if (process->GetID() == sender_process_host_id)
      continue;

    process->Send(new LayoutTestMsg_ReplicateLayoutTestRuntimeFlagsChanges(
        changed_layout_test_runtime_flags));
  }
}

void BlinkTestController::OnLayoutDumpResponse(RenderFrameHost* sender,
                                               const std::string& dump) {
  // Store the result.
  auto pair = frame_to_layout_dump_map_.insert(
      std::make_pair(sender->GetFrameTreeNodeId(), dump));
  bool insertion_took_place = pair.second;
  DCHECK(insertion_took_place);

  // See if we need to wait for more responses.
  pending_layout_dumps_--;
  DCHECK_LE(0, pending_layout_dumps_);
  if (pending_layout_dumps_ > 0)
    return;

  // Stitch the frame-specific results in the right order.
  std::string stitched_layout_dump;
  for (auto* render_frame_host : web_contents()->GetAllFrames()) {
    auto it =
        frame_to_layout_dump_map_.find(render_frame_host->GetFrameTreeNodeId());
    if (it != frame_to_layout_dump_map_.end()) {
      const std::string& dump = it->second;
      stitched_layout_dump.append(dump);
    }
  }

  // Continue finishing the test.
  RenderViewHost* render_view_host =
      main_window_->web_contents()->GetRenderViewHost();
  render_view_host->Send(new ShellViewMsg_LayoutDumpCompleted(
      render_view_host->GetRoutingID(), stitched_layout_dump));
}

void BlinkTestController::OnPrintMessage(const std::string& message) {
  printer_->AddMessageRaw(message);
}

void BlinkTestController::OnOverridePreferences(const WebPreferences& prefs) {
  should_override_prefs_ = true;
  prefs_ = prefs;

  // Notifies the main RenderViewHost that Blink preferences changed so
  // immediately apply the new settings and to avoid re-usage of cached
  // preferences that are now stale. RenderViewHost::UpdateWebkitPreferences is
  // not used here because it would send an unneeded preferences update to the
  // renderer.
  RenderViewHost* main_render_view_host =
      main_window_->web_contents()->GetRenderViewHost();
  main_render_view_host->OnWebkitPreferencesChanged();
}

void BlinkTestController::OnClearDevToolsLocalStorage() {
  ShellBrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  StoragePartition* storage_partition =
      BrowserContext::GetStoragePartition(browser_context, NULL);
  storage_partition->GetDOMStorageContext()->DeleteLocalStorage(
      content::LayoutTestDevToolsFrontend::GetDevToolsPathAsURL("")
          .GetOrigin());
}

void BlinkTestController::OnShowDevTools(const std::string& settings,
                                         const std::string& frontend_url) {
  if (!devtools_frontend_) {
    devtools_frontend_ = LayoutTestDevToolsFrontend::Show(
        main_window_->web_contents(), settings, frontend_url);
  } else {
    devtools_frontend_->ReuseFrontend(settings, frontend_url);
  }
  devtools_frontend_->Activate();
  devtools_frontend_->Focus();
}

void BlinkTestController::OnEvaluateInDevTools(
    int call_id, const std::string& script) {
  if (devtools_frontend_)
    devtools_frontend_->EvaluateInFrontend(call_id, script);
}

void BlinkTestController::OnCloseDevTools() {
  if (devtools_frontend_)
    devtools_frontend_->DisconnectFromTarget();
}

void BlinkTestController::OnGoToOffset(int offset) {
  main_window_->GoBackOrForward(offset);
}

void BlinkTestController::OnReload() {
  main_window_->Reload();
}

void BlinkTestController::OnLoadURLForFrame(const GURL& url,
                                            const std::string& frame_name) {
  main_window_->LoadURLForFrame(url, frame_name);
}

void BlinkTestController::OnCaptureSessionHistory() {
  std::vector<int> routing_ids;
  std::vector<std::vector<PageState> > session_histories;
  std::vector<unsigned> current_entry_indexes;

  RenderFrameHost* render_frame_host =
      main_window_->web_contents()->GetMainFrame();

  for (auto* window : Shell::windows()) {
    WebContents* web_contents = window->web_contents();
    // Only capture the history from windows in the same process as the main
    // window. During layout tests, we only use two processes when an
    // devtools window is open.
    auto* process = web_contents->GetMainFrame()->GetProcess();
    if (render_frame_host->GetProcess() != process)
      continue;

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

  RenderViewHost* render_view_host =
      main_window_->web_contents()->GetRenderViewHost();
  Send(new ShellViewMsg_SessionHistory(render_view_host->GetRoutingID(),
                                       routing_ids,
                                       session_histories,
                                       current_entry_indexes));
}

void BlinkTestController::OnCloseRemainingWindows() {
  DevToolsAgentHost::DetachAllClients();
  std::vector<Shell*> open_windows(Shell::windows());
  Shell* devtools_shell = devtools_frontend_ ?
      devtools_frontend_->frontend_shell() : NULL;
  for (size_t i = 0; i < open_windows.size(); ++i) {
    if (open_windows[i] != main_window_ && open_windows[i] != devtools_shell)
      open_windows[i]->Close();
  }
  base::RunLoop().RunUntilIdle();
}

void BlinkTestController::OnResetDone() {
  if (is_leak_detection_enabled_) {
    if (main_window_ && main_window_->web_contents()) {
      RenderViewHost* render_view_host =
          main_window_->web_contents()->GetRenderViewHost();
      render_view_host->Send(
          new ShellViewMsg_TryLeakDetection(render_view_host->GetRoutingID()));
    }
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

void BlinkTestController::OnLeakDetectionDone(
    const LeakDetectionResult& result) {
  if (!result.leaked) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    return;
  }

  printer_->AddErrorMessage(
      base::StringPrintf("#LEAK - renderer pid %d (%s)", current_pid_,
                         result.detail.c_str()));
  CHECK(!crash_when_leak_found_);

  DiscardMainWindow();
}

void BlinkTestController::OnSetBluetoothManualChooser(bool enable) {
  bluetooth_chooser_factory_.reset();
  if (enable) {
    bluetooth_chooser_factory_.reset(new LayoutTestBluetoothChooserFactory());
  }
}

void BlinkTestController::OnGetBluetoothManualChooserEvents() {
  if (!bluetooth_chooser_factory_) {
    printer_->AddErrorMessage(
        "FAIL: Must call setBluetoothManualChooser before "
        "getBluetoothManualChooserEvents.");
    return;
  }
  Send(new ShellViewMsg_ReplyBluetoothManualChooserEvents(
      main_window_->web_contents()->GetRenderViewHost()->GetRoutingID(),
      bluetooth_chooser_factory_->GetAndResetEvents()));
}

void BlinkTestController::OnSendBluetoothManualChooserEvent(
    const std::string& event_name,
    const std::string& argument) {
  if (!bluetooth_chooser_factory_) {
    printer_->AddErrorMessage(
        "FAIL: Must call setBluetoothManualChooser before "
        "sendBluetoothManualChooserEvent.");
    return;
  }
  BluetoothChooser::Event event;
  if (event_name == "cancelled") {
    event = BluetoothChooser::Event::CANCELLED;
  } else if (event_name == "selected") {
    event = BluetoothChooser::Event::SELECTED;
  } else if (event_name == "rescan") {
    event = BluetoothChooser::Event::RESCAN;
  } else {
    printer_->AddErrorMessage(base::StringPrintf(
        "FAIL: Unexpected sendBluetoothManualChooserEvent() event name '%s'.",
        event_name.c_str()));
    return;
  }
  bluetooth_chooser_factory_->SendEvent(event, argument);
}

mojom::LayoutTestControl* BlinkTestController::GetLayoutTestControlPtr(
    RenderFrameHost* frame) {
  if (layout_test_control_map_.find(frame) == layout_test_control_map_.end()) {
    frame->GetRemoteAssociatedInterfaces()->GetInterface(
        &layout_test_control_map_[frame]);
    layout_test_control_map_[frame].set_connection_error_handler(
        base::Bind(&BlinkTestController::HandleLayoutTestControlError,
                   base::Unretained(this), frame));
  }
  DCHECK(layout_test_control_map_[frame].get());
  return layout_test_control_map_[frame].get();
}

void BlinkTestController::HandleLayoutTestControlError(RenderFrameHost* frame) {
  layout_test_control_map_.erase(frame);
}

}  // namespace content
