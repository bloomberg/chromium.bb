// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BROWSER_WEBRUNNER_BROWSER_MAIN_PARTS_H_
#define FUCHSIA_BROWSER_WEBRUNNER_BROWSER_MAIN_PARTS_H_

#include <lib/fidl/cpp/binding.h>
#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "content/public/browser/browser_main_parts.h"
#include "fuchsia/browser/context_impl.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"

namespace display {
class Screen;
}

namespace webrunner {

class WebRunnerBrowserContext;

class WebRunnerBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit WebRunnerBrowserMainParts(zx::channel context_channel);
  ~WebRunnerBrowserMainParts() override;

  ContextImpl* context() { return context_service_.get(); }

  // content::BrowserMainParts overrides.
  void PreMainMessageLoopRun() override;
  void PreDefaultMainMessageLoopRun(base::OnceClosure quit_closure) override;
  void PostMainMessageLoopRun() override;

 private:
  zx::channel context_channel_;

  std::unique_ptr<display::Screen> screen_;
  std::unique_ptr<WebRunnerBrowserContext> browser_context_;
  std::unique_ptr<ContextImpl> context_service_;
  std::unique_ptr<fidl::Binding<chromium::web::Context>> context_binding_;

  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(WebRunnerBrowserMainParts);
};

}  // namespace webrunner

#endif  // FUCHSIA_BROWSER_WEBRUNNER_BROWSER_MAIN_PARTS_H_
