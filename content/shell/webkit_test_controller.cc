// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/webkit_test_controller.h"

#include <iostream>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/run_loop.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_context.h"
#include "content/shell/shell_content_browser_client.h"
#include "content/shell/shell_messages.h"
#include "content/shell/shell_switches.h"
#include "content/shell/webkit_test_helpers.h"
#include "webkit/support/webkit_support_gfx.h"

namespace content {

namespace {
const int kTestTimeoutMilliseconds = 30 * 1000;
}  // namespace

// WebKitTestResultPrinter ----------------------------------------------------

WebKitTestResultPrinter::WebKitTestResultPrinter(
    std::ostream* output, std::ostream* error)
    : state_(DURING_TEST),
      capture_text_only_(false),
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
  *output_ << "Content-Length: " << png_image.size() << "\n";
  output_->write(
      reinterpret_cast<const char*>(&png_image[0]), png_image.size());
}

void WebKitTestResultPrinter::PrintImageFooter() {
  if (state_ != IN_IMAGE_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    *error_ << "#EOF\n";
    output_->flush();
    error_->flush();
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
  *output_ << "Content-Length: " << audio_data.size() << "\n";
  output_->write(
      reinterpret_cast<const char*>(&audio_data[0]), audio_data.size());
}

void WebKitTestResultPrinter::PrintAudioFooter() {
  if (state_ != IN_AUDIO_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    *error_ << "#EOF\n";
    output_->flush();
    error_->flush();
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
  if (state_ != DURING_TEST)
    return;
  PrintTextHeader();
  *output_ << message << "\n";
  if (!capture_text_only_)
    *error_ << message << "\n";
  PrintTextFooter();
  PrintImageFooter();
  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

// WebKitTestController -------------------------------------------------------

WebKitTestController* WebKitTestController::instance_ = NULL;

// static
WebKitTestController* WebKitTestController::Get() {
  DCHECK(instance_);
  return instance_;
}

WebKitTestController::WebKitTestController() {
  CHECK(!instance_);
  instance_ = this;
  printer_.reset(new WebKitTestResultPrinter(&std::cout, &std::cerr));
  registrar_.Add(this,
                 NOTIFICATION_RENDERER_PROCESS_CREATED,
                 NotificationService::AllSources());
  ResetAfterLayoutTest();
}

WebKitTestController::~WebKitTestController() {
  DCHECK(CalledOnValidThread());
  CHECK(instance_ == this);
  if (main_window_)
    main_window_->Close();
  instance_ = NULL;
}

bool WebKitTestController::PrepareForLayoutTest(
    const GURL& test_url,
    const base::FilePath& current_working_directory,
    bool enable_pixel_dumping,
    const std::string& expected_pixel_hash) {
  DCHECK(CalledOnValidThread());
  current_working_directory_ = current_working_directory;
  enable_pixel_dumping_ = enable_pixel_dumping;
  expected_pixel_hash_ = expected_pixel_hash;
  test_url_ = test_url;
  printer_->reset();
  content::ShellBrowserContext* browser_context =
      static_cast<content::ShellContentBrowserClient*>(
          content::GetContentClient()->browser())->browser_context();
  if (test_url.spec().find("compositing/") != std::string::npos)
    is_compositing_test_ = true;
  gfx::Size initial_size;
  // The W3C SVG layout tests use a different size than the other layout tests.
  if (test_url.spec().find("W3C-SVG-1.1") != std::string::npos)
    initial_size = gfx::Size(480, 360);
  main_window_ = content::Shell::CreateNewWindow(
      browser_context,
      GURL(),
      NULL,
      MSG_ROUTING_NONE,
      initial_size);
  WebContentsObserver::Observe(main_window_->web_contents());
  main_window_->LoadURL(test_url);
  main_window_->web_contents()->GetRenderViewHost()->Focus();
  main_window_->web_contents()->GetRenderViewHost()->SetActive(true);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoTimeout)) {
    watchdog_.Reset(base::Bind(&WebKitTestController::TimeoutHandler,
                               base::Unretained(this)));
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        watchdog_.callback(),
        base::TimeDelta::FromMilliseconds(kTestTimeoutMilliseconds + 1000));
  }
  return true;
}

bool WebKitTestController::ResetAfterLayoutTest() {
  DCHECK(CalledOnValidThread());
  printer_->PrintTextFooter();
  printer_->PrintImageFooter();
  is_compositing_test_ = false;
  enable_pixel_dumping_ = false;
  expected_pixel_hash_.clear();
  test_url_ = GURL();
  prefs_ = webkit_glue::WebPreferences();
  should_override_prefs_ = false;
  watchdog_.Cancel();
  if (main_window_) {
    WebContentsObserver::Observe(NULL);
    main_window_ = NULL;
  }
  Shell::CloseAllWindows();
  Send(new ShellViewMsg_ResetAll);
  current_pid_ = base::kNullProcessId;
  return true;
}

void WebKitTestController::SetTempPath(const base::FilePath& temp_path) {
  temp_path_ = temp_path;
}

void WebKitTestController::RendererUnresponsive() {
  DCHECK(CalledOnValidThread());
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoTimeout))
    printer_->AddErrorMessage("#PROCESS UNRESPONSIVE - renderer");
}

void WebKitTestController::OverrideWebkitPrefs(
    webkit_glue::WebPreferences* prefs) {
  if (should_override_prefs_) {
    *prefs = prefs_;
  } else {
    ApplyLayoutTestDefaultPreferences(prefs);
    if (is_compositing_test_) {
      CommandLine& command_line = *CommandLine::ForCurrentProcess();
      if (command_line.HasSwitch(switches::kEnableSoftwareCompositing))
        prefs->accelerated_2d_canvas_enabled = true;
      prefs->accelerated_compositing_for_video_enabled = true;
      prefs->deferred_2d_canvas_enabled = true;
      prefs->mock_scrollbars_enabled = true;
    }
  }
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
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ShowDevTools, OnShowDevTools)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_CloseDevTools, OnCloseDevTools)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_GoToOffset, OnGoToOffset)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_Reload, OnReload)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_LoadURLForFrame, OnLoadURLForFrame)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_NotImplemented, OnNotImplemented)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebKitTestController::PluginCrashed(const base::FilePath& plugin_path,
                                         base::ProcessId plugin_pid) {
  DCHECK(CalledOnValidThread());
  printer_->AddErrorMessage(
      base::StringPrintf("#CRASHED - plugin (pid %d)", plugin_pid));
}

void WebKitTestController::RenderViewCreated(RenderViewHost* render_view_host) {
  DCHECK(CalledOnValidThread());
  // Might be kNullProcessHandle, in which case we will receive a notification
  // later when the RenderProcessHost was created.
  if (render_view_host->GetProcess()->GetHandle() != base::kNullProcessHandle)
    current_pid_ = base::GetProcId(render_view_host->GetProcess()->GetHandle());
  ShellViewMsg_SetTestConfiguration_Params params;
  params.current_working_directory = current_working_directory_;
  params.temp_path = temp_path_;
  params.test_url = test_url_;
  params.enable_pixel_dumping = enable_pixel_dumping_;
  params.layout_test_timeout = kTestTimeoutMilliseconds;
  params.allow_external_pages = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAllowExternalPages);
  params.expected_pixel_hash = expected_pixel_hash_;
  render_view_host->Send(new ShellViewMsg_SetTestConfiguration(
      render_view_host->GetRoutingID(), params));
}

void WebKitTestController::RenderViewGone(base::TerminationStatus status) {
  DCHECK(CalledOnValidThread());
  if (current_pid_ != base::kNullProcessId) {
    printer_->AddErrorMessage(std::string("#CRASHED - renderer (pid ") +
                              base::IntToString(current_pid_) + ")");
  } else {
    printer_->AddErrorMessage("#CRASHED - renderer");
  }
}

void WebKitTestController::WebContentsDestroyed(WebContents* web_contents) {
  DCHECK(CalledOnValidThread());
  main_window_ = NULL;
  printer_->AddErrorMessage("FAIL: main window was destroyed");
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

void WebKitTestController::TimeoutHandler() {
  DCHECK(CalledOnValidThread());
  printer_->AddErrorMessage(
      "FAIL: Timed out waiting for notifyDone to be called");
}

void WebKitTestController::OnTestFinished(bool did_timeout) {
  watchdog_.Cancel();
  if (!printer_->output_finished())
    printer_->PrintImageFooter();
  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
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

    bool success = false;
#if defined(OS_ANDROID)
    success = webkit_support::EncodeRGBAPNGWithChecksum(
        reinterpret_cast<const unsigned char*>(image.getPixels()),
        image.width(),
        image.height(),
        static_cast<int>(image.rowBytes()),
        discard_transparency,
        actual_pixel_hash,
        &png);
#else
    success = webkit_support::EncodeBGRAPNGWithChecksum(
        reinterpret_cast<const unsigned char*>(image.getPixels()),
        image.width(),
        image.height(),
        static_cast<int>(image.rowBytes()),
        discard_transparency,
        actual_pixel_hash,
        &png);
#endif
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

void WebKitTestController::OnOverridePreferences(
    const webkit_glue::WebPreferences& prefs) {
  should_override_prefs_ = true;
  prefs_ = prefs;
}

void WebKitTestController::OnShowDevTools() {
  main_window_->ShowDevTools();
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

void WebKitTestController::OnNotImplemented(
    const std::string& object_name,
    const std::string& property_name) {
  printer_->AddErrorMessage(
      std::string("FAIL: NOT IMPLEMENTED: ") +
      object_name + "." + property_name);
}

}  // namespace content
