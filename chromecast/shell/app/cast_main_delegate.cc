// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/app/cast_main_delegate.h"

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chromecast/common/cast_paths.h"
#include "chromecast/shell/browser/cast_content_browser_client.h"
#include "chromecast/shell/renderer/cast_content_renderer_client.h"
#include "content/public/common/content_switches.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromecast {
namespace shell {

CastMainDelegate::CastMainDelegate() {
}

CastMainDelegate::~CastMainDelegate() {
}

bool CastMainDelegate::BasicStartupComplete(int* exit_code) {
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
  // Time, process, and thread ID are available through logcat.
  logging::SetLogItems(true, true, false, false);

  RegisterPathProvider();

  content::SetContentClient(&content_client_);
  return false;
}

void CastMainDelegate::PreSandboxStartup() {
  InitializeResourceBundle();
}

void CastMainDelegate::ZygoteForked() {
}

// static
void CastMainDelegate::InitializeResourceBundle() {
  base::FilePath pak_file;
  base::FilePath pak_dir;

  PathService::Get(base::DIR_MODULE, &pak_dir);

  pak_file = pak_dir.Append(FILE_PATH_LITERAL("cast_shell.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
}

content::ContentBrowserClient* CastMainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(new CastContentBrowserClient);
  return browser_client_.get();
}

content::ContentRendererClient*
CastMainDelegate::CreateContentRendererClient() {
  renderer_client_.reset(new CastContentRendererClient);
  return renderer_client_.get();
}

}  // namespace shell
}  // namespace chromecast
