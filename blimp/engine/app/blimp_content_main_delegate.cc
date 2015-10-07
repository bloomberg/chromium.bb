// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_content_main_delegate.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "blimp/engine/browser/blimp_content_browser_client.h"
#include "blimp/engine/renderer/blimp_content_renderer_client.h"
#include "ui/base/resource/resource_bundle.h"

namespace blimp {
namespace engine {

BlimpContentMainDelegate::BlimpContentMainDelegate() {}

BlimpContentMainDelegate::~BlimpContentMainDelegate() {}

bool BlimpContentMainDelegate::BasicStartupComplete(int* exit_code) {
  content::SetContentClient(&content_client_);
  return false;
}

void BlimpContentMainDelegate::PreSandboxStartup() {
  InitializeResourceBundle();
}

void BlimpContentMainDelegate::InitializeResourceBundle() {
  base::FilePath pak_file;
  bool pak_file_valid = PathService::Get(base::DIR_MODULE, &pak_file);
  CHECK(pak_file_valid);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("blimp_engine.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
}

content::ContentBrowserClient*
BlimpContentMainDelegate::CreateContentBrowserClient() {
  DCHECK(!browser_client_);
  browser_client_.reset(new BlimpContentBrowserClient);
  return browser_client_.get();
}

content::ContentRendererClient*
BlimpContentMainDelegate::CreateContentRendererClient() {
  DCHECK(!renderer_client_);
  renderer_client_.reset(new BlimpContentRendererClient);
  return renderer_client_.get();
}

}  // namespace engine
}  // namespace blimp
