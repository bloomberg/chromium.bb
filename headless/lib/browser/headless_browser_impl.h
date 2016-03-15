// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_

#include "headless/public/headless_browser.h"

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "headless/lib/browser/headless_web_contents_impl.h"

namespace aura {
class WindowTreeHost;
}

namespace headless {

class HeadlessBrowserContext;
class HeadlessBrowserMainParts;

class HeadlessBrowserImpl : public HeadlessBrowser {
 public:
  HeadlessBrowserImpl(
      const base::Callback<void(HeadlessBrowser*)>& on_start_callback,
      const HeadlessBrowser::Options& options);
  ~HeadlessBrowserImpl() override;

  // HeadlessBrowser implementation:
  scoped_ptr<HeadlessWebContents> CreateWebContents(
      const gfx::Size& size) override;
  scoped_refptr<base::SingleThreadTaskRunner> BrowserMainThread()
      const override;

  void Shutdown() override;

  void set_browser_main_parts(HeadlessBrowserMainParts* browser_main_parts);
  HeadlessBrowserMainParts* browser_main_parts() const;

  HeadlessBrowserContext* browser_context() const;

  void RunOnStartCallback();

  const HeadlessBrowser::Options& options() const { return options_; }

  // Customize the options used by this headless browser instance. Note that
  // options which take effect before the message loop has been started (e.g.,
  // custom message pumps) cannot be set via this method.
  void SetOptionsForTesting(const HeadlessBrowser::Options& options);

 protected:
  base::Callback<void(HeadlessBrowser*)> on_start_callback_;
  HeadlessBrowser::Options options_;
  HeadlessBrowserMainParts* browser_main_parts_;  // Not owned.
  scoped_ptr<aura::WindowTreeHost> window_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_
