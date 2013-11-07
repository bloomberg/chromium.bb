// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "content/public/common/content_switches.h"
#include "webkit/browser/quota/quota_manager.h"

namespace content {

const base::FilePath::CharType kServiceWorkerDirectory[] =
    FILE_PATH_LITERAL("ServiceWorker");

ServiceWorkerContext::ServiceWorkerContext(
    const base::FilePath& path,
    quota::QuotaManagerProxy* quota_manager_proxy)
    : quota_manager_proxy_(quota_manager_proxy) {
  if (!path.empty())
    path_ = path.Append(kServiceWorkerDirectory);
}

bool ServiceWorkerContext::IsEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableServiceWorker);
}

ServiceWorkerContext::~ServiceWorkerContext() {}

}  // namespace content
