// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/service/cast_service_simple.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_web_contents_manager.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "net/base/filename_util.h"

namespace chromecast {
namespace shell {

namespace {

GURL GetStartupURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& args = command_line->GetArgs();

  if (args.empty())
    return GURL("http://www.google.com/");

  GURL url(args[0]);
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(
      base::MakeAbsoluteFilePath(base::FilePath(args[0])));
}

}  // namespace

CastServiceSimple::CastServiceSimple(content::BrowserContext* browser_context,
                                     PrefService* pref_service,
                                     CastWindowManager* window_manager)
    : CastService(browser_context, pref_service),
      window_manager_(window_manager),
      web_view_factory_(std::make_unique<CastWebViewFactory>(browser_context)),
      web_contents_manager_(
          std::make_unique<CastWebContentsManager>(browser_context,
                                                   web_view_factory_.get())) {
  DCHECK(window_manager_);
}

CastServiceSimple::~CastServiceSimple() {
}

void CastServiceSimple::InitializeInternal() {
  startup_url_ = GetStartupURL();
}

void CastServiceSimple::FinalizeInternal() {
}

void CastServiceSimple::StartInternal() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    return;
  }

  CastWebView::CreateParams params;
  params.delegate = this;
  params.enabled_for_dev = true;
  cast_web_view_ =
      web_contents_manager_->CreateWebView(params, nullptr, /* site_instance */
                                           nullptr,         /* extension */
                                           GURL() /* initial_url */);
  cast_web_view_->LoadUrl(startup_url_);
  cast_web_view_->CreateWindow(window_manager_, true /* is_visible */);
}

void CastServiceSimple::StopInternal() {
  if (cast_web_view_) {
    cast_web_view_->ClosePage(base::TimeDelta());
  }
  cast_web_view_.reset();
}

void CastServiceSimple::OnPageStopped(int error_code) {}

void CastServiceSimple::OnLoadingStateChanged(bool loading) {}

void CastServiceSimple::OnWindowDestroyed() {}

void CastServiceSimple::OnKeyEvent(const ui::KeyEvent& key_event) {}

bool CastServiceSimple::OnAddMessageToConsoleReceived(
    content::WebContents* source,
    int32_t level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  return false;
}

}  // namespace shell
}  // namespace chromecast
