// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_core.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "content/public/common/content_switches.h"
#include "webkit/browser/quota/quota_manager.h"

namespace content {

namespace {

const base::FilePath::CharType kServiceWorkerDirectory[] =
    FILE_PATH_LITERAL("ServiceWorker");

}  // namespace

ServiceWorkerContextCore::ServiceWorkerContextCore(
    const base::FilePath& user_data_directory,
    quota::QuotaManagerProxy* quota_manager_proxy)
    : quota_manager_proxy_(quota_manager_proxy) {
  if (!user_data_directory.empty())
    path_ = user_data_directory.Append(kServiceWorkerDirectory);
}

bool ServiceWorkerContextCore::IsEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableServiceWorker);
}

ServiceWorkerContextCore::~ServiceWorkerContextCore() {
}

}  // namespace content
