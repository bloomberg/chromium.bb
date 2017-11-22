// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
#define CONTENT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"
#include "content/shell/browser/shell_browser_context.h"

namespace net {
class NetLog;
}

namespace content {

class ShellBrowserMainParts : public BrowserMainParts {
 public:
  explicit ShellBrowserMainParts(const MainFunctionParams& parameters);
  ~ShellBrowserMainParts() override;

  // BrowserMainParts overrides.
  void PreEarlyInitialization() override;
  int PreCreateThreads() override;
  void PreMainMessageLoopStart() override;
  void PostMainMessageLoopStart() override;
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

  ShellBrowserContext* browser_context() { return browser_context_.get(); }
  ShellBrowserContext* off_the_record_browser_context() {
    return off_the_record_browser_context_.get();
  }

  net::NetLog* net_log() { return net_log_.get(); }

 protected:
  virtual void InitializeBrowserContexts();
  virtual void InitializeMessageLoopContext();

  void set_browser_context(ShellBrowserContext* context) {
    browser_context_.reset(context);
  }
  void set_off_the_record_browser_context(ShellBrowserContext* context) {
    off_the_record_browser_context_.reset(context);
  }

 private:
  void SetupFieldTrials();

  std::unique_ptr<net::NetLog> net_log_;
  std::unique_ptr<ShellBrowserContext> browser_context_;
  std::unique_ptr<ShellBrowserContext> off_the_record_browser_context_;

  // For running content_browsertests.
  const MainFunctionParams parameters_;
  bool run_message_loop_;

  // Statistical testing infrastructure for the entire browser. nullptr until
  // |SetupFieldTrials()| is called.
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserMainParts);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
