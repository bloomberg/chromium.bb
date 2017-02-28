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
#include "content/public/browser/web_contents.h"
#include "headless/lib/browser/headless_devtools_manager_delegate.h"
#include "headless/lib/browser/headless_web_contents_impl.h"

#if defined(USE_AURA)
#include "headless/lib/browser/headless_window_tree_host.h"

namespace aura {
namespace client {
class FocusClient;
}
}
#endif

namespace headless {

class HeadlessBrowserContextImpl;
class HeadlessBrowserMainParts;

class HeadlessBrowserImpl : public HeadlessBrowser {
 public:
  HeadlessBrowserImpl(
      const base::Callback<void(HeadlessBrowser*)>& on_start_callback,
      HeadlessBrowser::Options options);
  ~HeadlessBrowserImpl() override;

  // HeadlessBrowser implementation:
  HeadlessBrowserContext::Builder CreateBrowserContextBuilder() override;
  scoped_refptr<base::SingleThreadTaskRunner> BrowserFileThread()
      const override;
  scoped_refptr<base::SingleThreadTaskRunner> BrowserIOThread() const override;
  scoped_refptr<base::SingleThreadTaskRunner> BrowserMainThread()
      const override;

  void Shutdown() override;

  std::vector<HeadlessBrowserContext*> GetAllBrowserContexts() override;
  HeadlessWebContents* GetWebContentsForDevToolsAgentHostId(
      const std::string& devtools_agent_host_id) override;
  HeadlessBrowserContext* GetBrowserContextForId(
      const std::string& id) override;
  void SetDefaultBrowserContext(
      HeadlessBrowserContext* browser_context) override;
  HeadlessBrowserContext* GetDefaultBrowserContext() override;

  void set_browser_main_parts(HeadlessBrowserMainParts* browser_main_parts);
  HeadlessBrowserMainParts* browser_main_parts() const;

  void RunOnStartCallback();

  HeadlessBrowser::Options* options() { return &options_; }

  HeadlessBrowserContext* CreateBrowserContext(
      HeadlessBrowserContext::Builder* builder);
  // Close given |browser_context| and delete it
  // (all web contents associated with it go away too).
  void DestroyBrowserContext(HeadlessBrowserContextImpl* browser_context);

  base::WeakPtr<HeadlessBrowserImpl> GetWeakPtr();

  // All the methods that begin with Platform need to be implemented by the
  // platform specific headless implementation.
  // Helper for one time initialization of application
  void PlatformInitialize();
  void PlatformCreateWindow();
  void PlatformInitializeWebContents(const gfx::Size& initial_size,
                                     content::WebContents* web_contents);

 protected:
#if defined(USE_AURA)
  // TODO(eseckler): Currently one window and one window_tree_host
  // is used for all web contents. We should probably use one
  // window per web contents, but additional investigation is needed.
  std::unique_ptr<HeadlessWindowTreeHost> window_tree_host_;
  std::unique_ptr<aura::client::FocusClient> focus_client_;
#endif

  base::Callback<void(HeadlessBrowser*)> on_start_callback_;
  HeadlessBrowser::Options options_;
  HeadlessBrowserMainParts* browser_main_parts_;  // Not owned.

  std::unordered_map<std::string, std::unique_ptr<HeadlessBrowserContextImpl>>
      browser_contexts_;
  HeadlessBrowserContext* default_browser_context_;  // Not owned.

  base::WeakPtrFactory<HeadlessBrowserImpl> weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_IMPL_H_
