// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/lib/browser/headless_window_tree_client.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/size.h"

namespace headless {
namespace {

int RunContentMain(
    HeadlessBrowser::Options options,
    const base::Callback<void(HeadlessBrowser*)>& on_browser_start_callback) {
  content::ContentMainParams params(nullptr);
  params.argc = options.argc;
  params.argv = options.argv;

  // TODO(skyostil): Implement custom message pumps.
  DCHECK(!options.message_pump);

  std::unique_ptr<HeadlessBrowserImpl> browser(
      new HeadlessBrowserImpl(on_browser_start_callback, std::move(options)));
  headless::HeadlessContentMainDelegate delegate(std::move(browser));
  params.delegate = &delegate;
  return content::ContentMain(params);
}

}  // namespace

HeadlessBrowserImpl::HeadlessBrowserImpl(
    const base::Callback<void(HeadlessBrowser*)>& on_start_callback,
    HeadlessBrowser::Options options)
    : on_start_callback_(on_start_callback),
      options_(std::move(options)),
      browser_main_parts_(nullptr) {}

HeadlessBrowserImpl::~HeadlessBrowserImpl() {}

HeadlessBrowserContext::Builder
HeadlessBrowserImpl::CreateBrowserContextBuilder() {
  DCHECK(BrowserMainThread()->BelongsToCurrentThread());
  return HeadlessBrowserContext::Builder(this);
}

scoped_refptr<base::SingleThreadTaskRunner>
HeadlessBrowserImpl::BrowserMainThread() const {
  return content::BrowserThread::GetTaskRunnerForThread(
      content::BrowserThread::UI);
}

scoped_refptr<base::SingleThreadTaskRunner>
HeadlessBrowserImpl::BrowserFileThread() const {
  return content::BrowserThread::GetTaskRunnerForThread(
      content::BrowserThread::FILE);
}

void HeadlessBrowserImpl::Shutdown() {
  DCHECK(BrowserMainThread()->BelongsToCurrentThread());

  // DevToolsManagerDelegate owns some BrowserContexts. Tell it to delete them.
  if (devtools_manager_delegate()) {
    devtools_manager_delegate()->Shutdown();
  }

  // We need to close all WebContents here.
  std::vector<HeadlessWebContents*> all_web_contents = GetAllWebContents();
  for (HeadlessWebContents* web_contents : all_web_contents) {
    web_contents->Close();
  }
  DCHECK(web_contents_map_.empty());

  BrowserMainThread()->PostTask(FROM_HERE,
                                base::MessageLoop::QuitWhenIdleClosure());
}

std::vector<HeadlessWebContents*> HeadlessBrowserImpl::GetAllWebContents() {
  std::vector<HeadlessWebContents*> result;
  result.reserve(web_contents_map_.size());

  for (const auto& web_contents_pair : web_contents_map_) {
    result.push_back(web_contents_pair.second.get());
  }

  return result;
}

HeadlessBrowserMainParts* HeadlessBrowserImpl::browser_main_parts() const {
  DCHECK(BrowserMainThread()->BelongsToCurrentThread());
  return browser_main_parts_;
}

void HeadlessBrowserImpl::set_browser_main_parts(
    HeadlessBrowserMainParts* browser_main_parts) {
  DCHECK(!browser_main_parts_);
  browser_main_parts_ = browser_main_parts;
}

void HeadlessBrowserImpl::RunOnStartCallback() {
  DCHECK(aura::Env::GetInstance());
  window_tree_host_.reset(
      aura::WindowTreeHost::Create(gfx::Rect(options()->window_size)));
  window_tree_host_->InitHost();

  window_tree_client_.reset(
      new HeadlessWindowTreeClient(window_tree_host_->window()));

  on_start_callback_.Run(this);
  on_start_callback_ = base::Callback<void(HeadlessBrowser*)>();
}

HeadlessWebContents* HeadlessBrowserImpl::CreateWebContents(
    HeadlessWebContents::Builder* builder) {
  DCHECK(BrowserMainThread()->BelongsToCurrentThread());
  std::unique_ptr<HeadlessWebContentsImpl> headless_web_contents =
      HeadlessWebContentsImpl::Create(builder, window_tree_host_->window());
  if (!headless_web_contents)
    return nullptr;
  builder->browser_context_->RegisterWebContents(headless_web_contents.get());
  return RegisterWebContents(std::move(headless_web_contents));
}

HeadlessWebContentsImpl* HeadlessBrowserImpl::RegisterWebContents(
    std::unique_ptr<HeadlessWebContentsImpl> web_contents) {
  DCHECK(web_contents);
  HeadlessWebContentsImpl* unowned_web_contents = web_contents.get();
  web_contents_map_[unowned_web_contents->GetDevtoolsAgentHostId()] =
      std::move(web_contents);
  return unowned_web_contents;
}

void HeadlessBrowserImpl::DestroyWebContents(
    HeadlessWebContentsImpl* web_contents) {
  auto it = web_contents_map_.find(web_contents->GetDevtoolsAgentHostId());
  DCHECK(it != web_contents_map_.end());
  web_contents_map_.erase(it);
}

HeadlessDevToolsManagerDelegate*
HeadlessBrowserImpl::devtools_manager_delegate() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return devtools_manager_delegate_.get();
}

void HeadlessBrowserImpl::set_devtools_manager_delegate(
    base::WeakPtr<HeadlessDevToolsManagerDelegate> devtools_manager_delegate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  devtools_manager_delegate_ = devtools_manager_delegate;
}

HeadlessWebContents* HeadlessBrowserImpl::GetWebContentsForDevtoolsAgentHostId(
    const std::string& devtools_agent_host_id) {
  auto it = web_contents_map_.find(devtools_agent_host_id);
  if (it == web_contents_map_.end())
    return nullptr;
  return it->second.get();
}

void RunChildProcessIfNeeded(int argc, const char** argv) {
  base::CommandLine command_line(argc, argv);
  if (!command_line.HasSwitch(switches::kProcessType))
    return;

  HeadlessBrowser::Options::Builder builder(argc, argv);
  exit(RunContentMain(builder.Build(),
                      base::Callback<void(HeadlessBrowser*)>()));
}

int HeadlessBrowserMain(
    HeadlessBrowser::Options options,
    const base::Callback<void(HeadlessBrowser*)>& on_browser_start_callback) {
  DCHECK(!on_browser_start_callback.is_null());
#if DCHECK_IS_ON()
  // The browser can only be initialized once.
  static bool browser_was_initialized;
  DCHECK(!browser_was_initialized);
  browser_was_initialized = true;

  // Child processes should not end up here.
  base::CommandLine command_line(options.argc, options.argv);
  DCHECK(!command_line.HasSwitch(switches::kProcessType));
#endif
  return RunContentMain(std::move(options),
                        std::move(on_browser_start_callback));
}

}  // namespace headless
