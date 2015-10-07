// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/blimp_browser_main_parts.h"

#include "base/command_line.h"
#include "blimp/engine/browser/blimp_window.h"
#include "blimp/engine/ui/blimp_screen.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_module.h"
#include "net/log/net_log.h"
#include "url/gurl.h"

namespace blimp {
namespace engine {

namespace {

const char kDefaultURL[] = "https://www.google.com/";

GURL GetStartupURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& args = command_line->GetArgs();
  if (args.empty())
    return GURL(kDefaultURL);

  GURL url(args[0]);
  if (url.is_valid() && url.has_scheme())
    return url;

  return GURL(kDefaultURL);
}

}  // namespace

BlimpBrowserMainParts::BlimpBrowserMainParts(
    const content::MainFunctionParams& parameters) {}

BlimpBrowserMainParts::~BlimpBrowserMainParts() {}

void BlimpBrowserMainParts::PreMainMessageLoopRun() {
  net_log_.reset(new net::NetLog());
  browser_context_.reset(new BlimpBrowserContext(false, net_log_.get()));
  BlimpWindow::Create(browser_context_.get(), GetStartupURL(), nullptr,
                      gfx::Size());
}

int BlimpBrowserMainParts::PreCreateThreads() {
  screen_.reset(new BlimpScreen);
  DCHECK(!gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE));
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
  return 0;
}

void BlimpBrowserMainParts::PostMainMessageLoopRun() {
  browser_context_.reset();
}

}  // namespace engine
}  // namespace blimp
