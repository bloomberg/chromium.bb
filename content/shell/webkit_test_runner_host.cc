// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/webkit_test_runner_host.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_context.h"
#include "content/shell/shell_content_browser_client.h"
#include "content/shell/shell_messages.h"
#include "content/shell/shell_switches.h"
#include "webkit/support/webkit_support_gfx.h"

namespace content {

namespace {
const int kTestTimeoutMilliseconds = 30 * 1000;
}  // namespace

// WebKitTestController -------------------------------------------------------

WebKitTestController* WebKitTestController::instance_ = NULL;

// static
WebKitTestController* WebKitTestController::Get() {
  DCHECK(!instance_ || instance_->CalledOnValidThread());
  return instance_;
}

WebKitTestController::WebKitTestController() {
  CHECK(!instance_);
  instance_ = this;

  content::ShellBrowserContext* browser_context =
      static_cast<content::ShellContentBrowserClient*>(
          content::GetContentClient()->browser())->browser_context();
  main_window_ = content::Shell::CreateNewWindow(
      browser_context,
      GURL("about:blank"),
      NULL,
      MSG_ROUTING_NONE,
      NULL);
  Observe(main_window_->web_contents());
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
    bool enable_pixel_dumping,
    const std::string& expected_pixel_hash) {
  DCHECK(CalledOnValidThread());
  if (!main_window_)
    return false;
  in_test_ = true;
  enable_pixel_dumping_ = enable_pixel_dumping;
  expected_pixel_hash_ = expected_pixel_hash;
  main_window_->LoadURL(test_url);
  return true;
}

bool WebKitTestController::ResetAfterLayoutTest() {
  DCHECK(CalledOnValidThread());
  in_test_ = false;
  pumping_messages_ = false;
  enable_pixel_dumping_ = false;
  expected_pixel_hash_.clear();
  captured_dump_ = false;
  finished_text_block_ = false;
  finished_pixel_block_ = false;
  dump_as_text_ = false;
  dump_child_frames_ = false;
  is_printing_ = false;
  should_stay_on_page_after_handling_before_unload_ = false;
  wait_until_done_ = false;
  watchdog_.Cancel();
  if (main_window_) {
    if (main_window_->web_contents()->GetController().GetEntryCount() > 0) {
      // Reset the WebContents for the next test.
      // TODO(jochen): Reset it more thoroughly.
      main_window_->web_contents()->GetController().GoToIndex(0);
    }
    // Wait for the main_window_ to finish loading.
    pumping_messages_ = true;
    base::RunLoop run_loop;
    run_loop.Run();
    pumping_messages_ = false;
  }
  return main_window_ != NULL;
}

void WebKitTestController::NotifyDone() {
  if (!wait_until_done_)
    return;
  watchdog_.Cancel();
  CaptureDump();
}

void WebKitTestController::WaitUntilDone() {
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

void WebKitTestController::NotImplemented(
    const std::string& object_name,
    const std::string& property_name) {
  if (captured_dump_)
    return;
  printf("FAIL: NOT IMPLEMENTED: %s.%s\n",
         object_name.c_str(), property_name.c_str());
  fprintf(stderr, "FAIL: NOT IMPLEMENTED: %s.%s\n",
          object_name.c_str(), property_name.c_str());
  watchdog_.Cancel();
  FinishRemainingBlocks();
}

bool WebKitTestController::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebKitTestController, message)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_DidFinishLoad, OnDidFinishLoad)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_TextDump, OnTextDump)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_ImageDump, OnImageDump)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebKitTestController::RenderViewGone(base::TerminationStatus status) {
  if (!in_test_)
    return;
  printf("FAIL: renderer died\n");
  fprintf(stderr, "FAIL: renderer died\n");
  watchdog_.Cancel();
  FinishRemainingBlocks();
}

void WebKitTestController::WebContentsDestroyed(WebContents* web_contents) {
  main_window_ = NULL;
  if (!in_test_)
    return;
  printf("FAIL: main window was destroyed\n");
  fprintf(stderr, "FAIL: main window was destroyed\n");
  watchdog_.Cancel();
  FinishRemainingBlocks();
}

void WebKitTestController::FinishRemainingBlocks() {
  if (!finished_text_block_) {
    printf("#EOF\n");
    fprintf(stderr, "#EOF\n");
  }
  if (!finished_pixel_block_)
    printf("#EOF\n");
  finished_text_block_ = true;
  finished_pixel_block_ = true;
  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

void WebKitTestController::CaptureDump() {
  if (captured_dump_ || !main_window_ || !in_test_)
    return;
  captured_dump_ = true;

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
  printf("FAIL: Timed out waiting for notifyDone to be called\n");
  fprintf(stderr, "FAIL: Timed out waiting for notifyDone to be called\n");
  FinishRemainingBlocks();
}

void WebKitTestController::OnDidFinishLoad() {
  if (pumping_messages_) {
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    return;
  }
  if (wait_until_done_)
    return;
  CaptureDump();
}

void WebKitTestController::OnImageDump(
    const std::string& actual_pixel_hash,
    const SkBitmap& image) {
  SkAutoLockPixels image_lock(image);

  if (!finished_pixel_block_) {
    printf("\nActualHash: %s\n", actual_pixel_hash.c_str());
    if (!expected_pixel_hash_.empty())
      printf("\nExpectedHash: %s\n", expected_pixel_hash_.c_str());

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
      if (success) {
        printf("Content-Type: image/png\n");
        printf("Content-Length: %u\n", static_cast<unsigned>(png.size()));
        fwrite(&png[0], 1, png.size(), stdout);
      }
    }
  }
  FinishRemainingBlocks();
}

void WebKitTestController::OnTextDump(const std::string& dump) {
  if (!finished_text_block_) {
    printf("%s#EOF\n", dump.c_str());
    fprintf(stderr, "#EOF\n");
    finished_text_block_ = true;
  }

  if (dump_as_text_ || !enable_pixel_dumping_)
    FinishRemainingBlocks();
}

// WebKitTestRunnerHost -------------------------------------------------------

WebKitTestRunnerHost::WebKitTestRunnerHost(
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host) {
}

WebKitTestRunnerHost::~WebKitTestRunnerHost() {
}

bool WebKitTestRunnerHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebKitTestRunnerHost, message)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_NotifyDone, OnNotifyDone)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_DumpAsText, OnDumpAsText)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_DumpChildFramesAsText,
                        OnDumpChildFramesAsText)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_SetPrinting, OnSetPrinting)
    IPC_MESSAGE_HANDLER(
        ShellViewHostMsg_SetShouldStayOnPageAfterHandlingBeforeUnload,
        OnSetShouldStayOnPageAfterHandlingBeforeUnload)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_WaitUntilDone, OnWaitUntilDone)
    IPC_MESSAGE_HANDLER(ShellViewHostMsg_NotImplemented, OnNotImplemented)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebKitTestRunnerHost::OnNotifyDone() {
  WebKitTestController::Get()->NotifyDone();
}

void WebKitTestRunnerHost::OnDumpAsText() {
  WebKitTestController::Get()->set_dump_as_text(true);
}

void WebKitTestRunnerHost::OnSetPrinting() {
  WebKitTestController::Get()->set_is_printing(true);
}

void WebKitTestRunnerHost::OnSetShouldStayOnPageAfterHandlingBeforeUnload(
    bool should_stay_on_page) {
  WebKitTestController* controller = WebKitTestController::Get();
  controller->set_should_stay_on_page_after_handling_before_unload(
      should_stay_on_page);
}

void WebKitTestRunnerHost::OnDumpChildFramesAsText() {
  WebKitTestController::Get()->set_dump_child_frames(true);
}

void WebKitTestRunnerHost::OnWaitUntilDone() {
  WebKitTestController::Get()->WaitUntilDone();
}

void WebKitTestRunnerHost::OnNotImplemented(
    const std::string& object_name,
    const std::string& property_name) {
  WebKitTestController::Get()->NotImplemented(object_name, property_name);
}

}  // namespace content
