// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_browser_main.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_context.h"
#include "content/shell/shell_content_browser_client.h"
#include "net/base/net_module.h"
#include "ui/base/clipboard/clipboard.h"

namespace content {

static GURL GetStartupURL() {
  const CommandLine::StringVector& args =
      CommandLine::ForCurrentProcess()->GetArgs();
  if (args.empty())
    return GURL("http://www.google.com/");

  return GURL(args[0]);
}

ShellBrowserMainParts::ShellBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : BrowserMainParts() {
  ShellContentBrowserClient* shell_browser_client =
      static_cast<ShellContentBrowserClient*>(
          content::GetContentClient()->browser());
  shell_browser_client->set_shell_browser_main_parts(this);
}

ShellBrowserMainParts::~ShellBrowserMainParts() {
  base::ThreadRestrictions::SetIOAllowed(true);
    io_thread()->message_loop()->PostTask(
        FROM_HERE, base::IgnoreReturn<bool>(
            base::Bind(&base::ThreadRestrictions::SetIOAllowed, true)));

  browser_context_.reset();

  resource_dispatcher_host_->download_file_manager()->Shutdown();
  resource_dispatcher_host_->save_file_manager()->Shutdown();
  resource_dispatcher_host_->Shutdown();
  io_thread_.reset();
  cache_thread_.reset();
  process_launcher_thread_.reset();
  file_thread_.reset();
  resource_dispatcher_host_.reset();  // Kills WebKit thread.
  db_thread_.reset();
}

void ShellBrowserMainParts::PreMainMessageLoopRun() {
  db_thread_.reset(new BrowserProcessSubThread(BrowserThread::DB));
  db_thread_->Start();
  file_thread_.reset(new BrowserProcessSubThread(BrowserThread::FILE));
  file_thread_->Start();
  process_launcher_thread_.reset(
      new BrowserProcessSubThread(BrowserThread::PROCESS_LAUNCHER));
  process_launcher_thread_->Start();

  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;

  cache_thread_.reset(new BrowserProcessSubThread(BrowserThread::CACHE));
  cache_thread_->StartWithOptions(options);
  io_thread_.reset(new BrowserProcessSubThread(BrowserThread::IO));
  io_thread_->StartWithOptions(options);

  browser_context_.reset(new ShellBrowserContext(this));

  Shell::PlatformInitialize();
  net::NetModule::SetResourceProvider(Shell::PlatformResourceProvider);

  Shell::CreateNewWindow(browser_context_.get(),
                         GetStartupURL(),
                         NULL,
                         MSG_ROUTING_NONE,
                         NULL);

}

bool ShellBrowserMainParts::MainMessageLoopRun(int* result_code) {
  return false;
}

ResourceDispatcherHost* ShellBrowserMainParts::GetResourceDispatcherHost() {
  if (!resource_dispatcher_host_.get()) {
    ResourceQueue::DelegateSet resource_queue_delegates;
    resource_dispatcher_host_.reset(
        new ResourceDispatcherHost(resource_queue_delegates));
    resource_dispatcher_host_->Initialize();
  }
  return resource_dispatcher_host_.get();
}

ui::Clipboard* ShellBrowserMainParts::GetClipboard() {
  if (!clipboard_.get())
    clipboard_.reset(new ui::Clipboard());
  return clipboard_.get();
}

}  // namespace
