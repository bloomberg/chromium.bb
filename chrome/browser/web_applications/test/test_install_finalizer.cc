// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/test/test_install_finalizer.h"

#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"

namespace web_app {

TestInstallFinalizer::TestInstallFinalizer() {}

TestInstallFinalizer::~TestInstallFinalizer() = default;

void TestInstallFinalizer::FinalizeInstall(
    const WebApplicationInfo& web_app_info,
    InstallFinalizedCallback callback) {
  AppId app_id = GenerateAppIdFromURL(web_app_info.app_url);
  if (next_app_id_.has_value()) {
    app_id = next_app_id_.value();
    next_app_id_.reset();
  }

  InstallResultCode code = InstallResultCode::kSuccess;
  if (next_result_code_.has_value()) {
    code = next_result_code_.value();
    next_result_code_.reset();
  }

  // Store a copy for inspecting in tests.
  web_app_info_copy_ = std::make_unique<WebApplicationInfo>(web_app_info);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), app_id, code));
}

void TestInstallFinalizer::CreateOsShortcuts(const AppId& app_id) {
  ++num_create_os_shortcuts_calls_;
}

void TestInstallFinalizer::ReparentTab(const WebApplicationInfo& web_app_info,
                                       const AppId& app_id,
                                       content::WebContents* web_contents) {
  ++num_reparent_tab_num_calls_;
}

void TestInstallFinalizer::SetNextFinalizeInstallResult(
    const AppId& app_id,
    InstallResultCode code) {
  next_app_id_ = app_id;
  next_result_code_ = code;
}

}  // namespace web_app
