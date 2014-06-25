// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/mojo_web_ui_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/mojo_web_ui_handler.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/service_registry.h"
#include "mojo/public/cpp/system/core.h"

MojoWebUIController::MojoWebUIController(content::WebUI* contents)
    : WebUIController(contents), mojo_data_source_(NULL), weak_factory_(this) {
}

MojoWebUIController::~MojoWebUIController() {
}

void MojoWebUIController::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  render_view_host->AllowBindings(content::BINDINGS_POLICY_WEB_UI);
  render_view_host->GetMainFrame()->GetServiceRegistry()->AddService(
      "webui_controller",
      base::Bind(&MojoWebUIController::CreateAndStoreUIHandler,
                 weak_factory_.GetWeakPtr()));
}

void MojoWebUIController::AddMojoResourcePath(const std::string& path,
                                              int resource_id) {
  if (!mojo_data_source_) {
    mojo_data_source_ = content::WebUIDataSource::AddMojoDataSource(
        Profile::FromWebUI(web_ui()));
  }
  mojo_data_source_->AddResourcePath(path, resource_id);
}

void MojoWebUIController::CreateAndStoreUIHandler(
    mojo::ScopedMessagePipeHandle handle) {
  ui_handler_ = CreateUIHandler(handle.Pass());
}
