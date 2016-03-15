// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#include "base/thread_task_runner_handle.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "headless/lib/browser/headless_browser_context.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

HeadlessBrowserImpl::HeadlessBrowserImpl(
    const base::Callback<void(HeadlessBrowser*)>& on_start_callback,
    const HeadlessBrowser::Options& options)
    : on_start_callback_(on_start_callback),
      options_(options),
      browser_main_parts_(nullptr) {
  DCHECK(!on_start_callback_.is_null());
}

HeadlessBrowserImpl::~HeadlessBrowserImpl() {}

scoped_ptr<HeadlessWebContents> HeadlessBrowserImpl::CreateWebContents(
    const gfx::Size& size) {
  DCHECK(BrowserMainThread()->BelongsToCurrentThread());
  return make_scoped_ptr(new HeadlessWebContentsImpl(
      browser_context(), window_tree_host_->window(), size));
}

scoped_refptr<base::SingleThreadTaskRunner>
HeadlessBrowserImpl::BrowserMainThread() const {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::UI);
}

void HeadlessBrowserImpl::Shutdown() {
  DCHECK(BrowserMainThread()->BelongsToCurrentThread());
  BrowserMainThread()->PostTask(FROM_HERE,
                                base::MessageLoop::QuitWhenIdleClosure());
}

HeadlessBrowserContext* HeadlessBrowserImpl::browser_context() const {
  DCHECK(BrowserMainThread()->BelongsToCurrentThread());
  DCHECK(browser_main_parts());
  return browser_main_parts()->browser_context();
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
  window_tree_host_.reset(aura::WindowTreeHost::Create(gfx::Rect()));
  window_tree_host_->InitHost();

  on_start_callback_.Run(this);
  on_start_callback_ = base::Callback<void(HeadlessBrowser*)>();
}

void HeadlessBrowserImpl::SetOptionsForTesting(
    const HeadlessBrowser::Options& options) {
  options_ = options;
  browser_context()->SetOptionsForTesting(options);
}

int HeadlessBrowserMain(
    const HeadlessBrowser::Options& options,
    const base::Callback<void(HeadlessBrowser*)>& on_browser_start_callback) {
  scoped_ptr<HeadlessBrowserImpl> browser(
      new HeadlessBrowserImpl(on_browser_start_callback, options));

  // TODO(skyostil): Implement custom message pumps.
  DCHECK(!options.message_pump);

  headless::HeadlessContentMainDelegate delegate(std::move(browser));
  content::ContentMainParams params(&delegate);
  params.argc = options.argc;
  params.argv = options.argv;
  return content::ContentMain(params);
}

}  // namespace headless
