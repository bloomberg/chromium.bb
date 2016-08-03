// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_

#include "headless/public/headless_browser.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "headless/lib/browser/headless_devtools_manager_delegate.h"
#include "headless/lib/browser/headless_web_contents_impl.h"

namespace aura {
class WindowTreeHost;

namespace client {
class WindowTreeClient;
}
}

namespace headless {

class HeadlessBrowserContext;
class HeadlessBrowserMainParts;

class HeadlessBrowserImpl : public HeadlessBrowser {
 public:
  HeadlessBrowserImpl(
      const base::Callback<void(HeadlessBrowser*)>& on_start_callback,
      HeadlessBrowser::Options options);
  ~HeadlessBrowserImpl() override;

  // HeadlessBrowser implementation:
  HeadlessBrowserContext::Builder CreateBrowserContextBuilder() override;
  scoped_refptr<base::SingleThreadTaskRunner> BrowserMainThread()
      const override;
  scoped_refptr<base::SingleThreadTaskRunner> BrowserFileThread()
      const override;

  void Shutdown() override;

  std::vector<HeadlessWebContents*> GetAllWebContents() override;
  HeadlessWebContents* GetWebContentsForDevtoolsAgentHostId(
      const std::string& devtools_agent_host_id) override;

  void set_browser_main_parts(HeadlessBrowserMainParts* browser_main_parts);
  HeadlessBrowserMainParts* browser_main_parts() const;

  void RunOnStartCallback();

  HeadlessBrowser::Options* options() { return &options_; }

  HeadlessWebContents* CreateWebContents(HeadlessWebContents::Builder* builder);
  HeadlessWebContentsImpl* RegisterWebContents(
      std::unique_ptr<HeadlessWebContentsImpl> web_contents);

  // Close given |web_contents| and delete it.
  void DestroyWebContents(HeadlessWebContentsImpl* web_contents);

  HeadlessDevToolsManagerDelegate* devtools_manager_delegate() const;
  void set_devtools_manager_delegate(
      base::WeakPtr<HeadlessDevToolsManagerDelegate>);

 protected:
  base::Callback<void(HeadlessBrowser*)> on_start_callback_;
  HeadlessBrowser::Options options_;
  HeadlessBrowserMainParts* browser_main_parts_;  // Not owned.
  std::unique_ptr<aura::WindowTreeHost> window_tree_host_;
  std::unique_ptr<aura::client::WindowTreeClient> window_tree_client_;

  std::unordered_map<std::string, std::unique_ptr<HeadlessWebContents>>
      web_contents_map_;

  base::WeakPtr<HeadlessDevToolsManagerDelegate> devtools_manager_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_
