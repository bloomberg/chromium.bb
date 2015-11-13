// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/mojo_web_ui_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/mojo_web_ui_handler.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "mojo/public/cpp/system/core.h"

MojoWebUIControllerBase::MojoWebUIControllerBase(content::WebUI* contents)
    : WebUIController(contents), mojo_data_source_(NULL) {
}

MojoWebUIControllerBase::~MojoWebUIControllerBase() {
}

void MojoWebUIControllerBase::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  render_view_host->AllowBindings(content::BINDINGS_POLICY_WEB_UI);
}

void MojoWebUIControllerBase::AddMojoResourcePath(const std::string& path,
                                                  int resource_id) {
  if (!mojo_data_source_) {
    mojo_data_source_ = content::WebUIDataSource::AddMojoDataSource(
        Profile::FromWebUI(web_ui()));
  }
  mojo_data_source_->AddResourcePath(path, resource_id);
}
