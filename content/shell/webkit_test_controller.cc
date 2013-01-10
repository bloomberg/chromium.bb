// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/webkit_test_controller.h"

#include <iostream>

#include "base/command_line.h"
#include "base/file_util.h"
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
#include "webkit/fileapi/isolated_context.h"
#include "webkit/support/webkit_support_gfx.h"

namespace content {

namespace {
const int kTestTimeoutMilliseconds = 30 * 1000;
}  // namespace

// WebKitTestResultPrinter ----------------------------------------------------

WebKitTestResultPrinter::WebKitTestResultPrinter(
    std::ostream* output, std::ostream* error)
    : state_(BEFORE_TEST),
      capture_text_only_(false),
      output_(output),
      error_(error) {
}

WebKitTestResultPrinter::~WebKitTestResultPrinter() {
}

void WebKitTestResultPrinter::PrintTextHeader() {
  DCHECK_EQ(state_, BEFORE_TEST);
  if (!capture_text_only_)
    *output_ << "Content-Type: text/plain\n";
  state_ = IN_TEXT_BLOCK;
}

void WebKitTestResultPrinter::PrintTextBlock(const std::string& block) {
  DCHECK_EQ(state_, IN_TEXT_BLOCK);
  *output_ << block;
}

void WebKitTestResultPrinter::PrintTextFooter() {
  if (state_ != IN_TEXT_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    *error_ << "#EOF\n";
    output_->flush();
    error_->flush();
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
    output_->flush();
  }
  state_ = AFTER_TEST;
}

void WebKitTestResultPrinter::AddMessage(const std::string& message) {
  AddMessageRaw(message + "\n");
}

void WebKitTestResultPrinter::AddMessageRaw(const std::string& message) {
  if (state_ != IN_TEXT_BLOCK)
    return;
  *output_ << message;
}

void WebKitTestResultPrinter::AddErrorMessage(const std::string& message) {
  if (state_ != IN_TEXT_BLOCK)
    return;
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
    const FilePath& current_working_directory,
    bool enable_pixel_dumping,
    const std::string& expected_pixel_hash) {
  DCHECK(CalledOnValidThread());
  current_working_directory_ = current_working_directory;
  enable_pixel_dumping_ = enable_pixel_dumping;
  expected_pixel_hash_ = expected_pixel_hash;
  printer_->reset();
  printer_->PrintTextHeader();
  content::ShellBrowserContext* browser_context =
      static_cast<content::ShellContentBrowserClient*>(
          content::GetContentClient()->browser())->browser_context();
  main_window_ = content::Shell::CreateNewWindow(
      browser_context,
      GURL(),
      NULL,
      MSG_ROUTING_NONE,
      NULL);
  WebContentsObserver::Observe(main_window_->web_contents());
  main_window_->LoadURL(test_url);
  if (test_url.spec().find("/dumpAsText/") != std::string::npos ||
      test_url.spec().find("\\dumpAsText\\") != std::string::npos) {
    dump_as_text_ = true;
    enable_pixel_dumping_ = false;
  }
  if (test_url.spec().find("/inspector/") != std::string::npos ||
      test_url.spec().find("\\inspector\\") != std::string::npos) {
    main_window_->ShowDevTools();
  }
  main_window_->web_contents()->GetRenderViewHost()->Focus();
  main_window_->web_contents()->GetRenderViewHost()->SetActive(true);
  return true;
}

bool WebKitTestController::ResetAfterLayoutTest() {
  DCHECK(CalledOnValidThread());
  printer_->PrintTextFooter();
  printer_->PrintImageFooter();
  enable_pixel_dumping_ = false;
  expected_pixel_hash_.clear();
  captured_dump_ = false;
  dump_as_text_ = false;
  dump_child_frames_ = false;
  is_printing_ = false;
  should_stay_on_page_after_handling_before_unload_ = false;
  wait_until_done_ = false;
  did_finish_load_ = false;
  prefs_ = webkit_glue::WebPreferences();
  should_override_prefs_ = false;
  {
    base::AutoLock lock(lock_);
    can_open_windows_ = false;
  }
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

void WebKitTestController::RendererUnresponsive() {
  DCHECK(CalledOnValidThread());
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoTimeout))
    printer_->AddErrorMessage("#PROCESS UNRESPONSIVE - renderer");
}

void WebKitTestController::OverrideWebkitPrefs(
    webkit_glue::WebPreferences* prefs) {
  if (should_override_prefs_)
    *prefs = prefs_;
  else
    ApplyLayoutTestDefaultPreferences(prefs);
}

bool WebKitTestController::CanOpenWindows() const {
  base::AutoLock lock(lock_);
  return can_open_windows_;
}

bool WebKitTestController::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebKitTestController, message)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_DidFinishLoad, OnDidFinishLoad)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_PrintMessage, OnPrintMessage)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ReadFileToString, OnReadFileToString)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_RegisterIsolatedFileSystem,
                        OnRegisterIsolatedFileSystem)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_TextDump, OnTextDump)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ImageDump, OnImageDump)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_OverridePreferences,
                        OnOverridePreferences)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_NotifyDone, OnNotifyDone)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_DumpAsText, OnDumpAsText)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_DumpChildFramesAsText,
                        OnDumpChildFramesAsText)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_SetPrinting, OnSetPrinting)
    IPC_MESSAGE_HANDLER(
        ShellViewHostMsg_SetShouldStayOnPageAfterHandlingBeforeUnload,
        OnSetShouldStayOnPageAfterHandlingBeforeUnload)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_WaitUntilDone, OnWaitUntilDone)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_CanOpenWindows, OnCanOpenWindows)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ShowWebInspector, OnShowWebInspector)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_CloseWebInspector, OnCloseWebInspector)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_NotImplemented, OnNotImplemented)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebKitTestController::PluginCrashed(const FilePath& plugin_path,
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
  render_view_host->Send(new ShellViewMsg_SetCurrentWorkingDirectory(
      render_view_host->GetRoutingID(), current_working_directory_));
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

void WebKitTestController::CaptureDump() {
  if (captured_dump_ || !main_window_ || !printer_->in_text_block())
    return;
  captured_dump_ = true;

  if (main_window_->web_contents()->GetContentsMimeType() == "text/plain") {
    dump_as_text_ = true;
    enable_pixel_dumping_ = false;
  }

  RenderViewHost* render_view_host =
      main_window_->web_contents()->GetRenderViewHost();

  render_view_host->Send(new ShellViewMsg_CaptureTextDump(
      render_view_host->GetRoutingID(),
      dump_as_text_,
      is_printing_,
      dump_child_frames_));
  if (!dump_as_text_ && enable_pixel_dumping_) {
    render_view_host->Send(new ShellViewMsg_CaptureImageDump(
        render_view_host->GetRoutingID(),
        expected_pixel_hash_));
  }
}

void WebKitTestController::TimeoutHandler() {
  DCHECK(CalledOnValidThread());
  printer_->AddErrorMessage(
      "FAIL: Timed out waiting for notifyDone to be called");
}

void WebKitTestController::OnDidFinishLoad() {
  did_finish_load_ = true;
  if (wait_until_done_)
    return;
  CaptureDump();
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
  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

void WebKitTestController::OnTextDump(const std::string& dump) {
  printer_->PrintTextBlock(dump);
  printer_->PrintTextFooter();
  if (dump_as_text_ || !enable_pixel_dumping_) {
    printer_->PrintImageFooter();
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }
}

void WebKitTestController::OnPrintMessage(const std::string& message) {
  printer_->AddMessageRaw(message);
}

void WebKitTestController::OnReadFileToString(const FilePath& local_file,
                                              std::string* contents) {
  file_util::ReadFileToString(local_file, contents);
}

void WebKitTestController::OnOverridePreferences(
    const webkit_glue::WebPreferences& prefs) {
  should_override_prefs_ = true;
  prefs_ = prefs;
}

void WebKitTestController::OnNotifyDone() {
  if (!wait_until_done_)
    return;
  watchdog_.Cancel();
  if (!did_finish_load_) {
    wait_until_done_ = false;
    return;
  }
  CaptureDump();
}

void WebKitTestController::OnDumpAsText() {
  dump_as_text_ = true;
}

void WebKitTestController::OnSetPrinting() {
  is_printing_ = true;
}

void WebKitTestController::OnSetShouldStayOnPageAfterHandlingBeforeUnload(
    bool should_stay_on_page) {
  should_stay_on_page_after_handling_before_unload_ = should_stay_on_page;
}

void WebKitTestController::OnDumpChildFramesAsText() {
  dump_child_frames_ = true;
}

void WebKitTestController::OnWaitUntilDone() {
  if (wait_until_done_)
    return;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoTimeout)) {
    watchdog_.Reset(base::Bind(&WebKitTestController::TimeoutHandler,
                               base::Unretained(this)));
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        watchdog_.callback(),
        base::TimeDelta::FromMilliseconds(kTestTimeoutMilliseconds));
  }
  wait_until_done_ = true;
}

void WebKitTestController::OnCanOpenWindows() {
  base::AutoLock lock(lock_);
  can_open_windows_ = true;
}

void WebKitTestController::OnShowWebInspector() {
  main_window_->ShowDevTools();
}

void WebKitTestController::OnCloseWebInspector() {
  main_window_->CloseDevTools();
}

void WebKitTestController::OnRegisterIsolatedFileSystem(
    const std::vector<FilePath>& absolute_filenames,
    std::string* filesystem_id) {
  fileapi::IsolatedContext::FileInfoSet files;
  for (size_t i = 0; i < absolute_filenames.size(); ++i)
    files.AddPath(absolute_filenames[i], NULL);
  *filesystem_id =
      fileapi::IsolatedContext::GetInstance()->RegisterDraggedFileSystem(files);
}

void WebKitTestController::OnNotImplemented(
    const std::string& object_name,
    const std::string& property_name) {
  printer_->AddErrorMessage(
      std::string("FAIL: NOT IMPLEMENTED: ") +
      object_name + "." + property_name);
}

}  // namespace content
