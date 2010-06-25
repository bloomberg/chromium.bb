// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_process.h"

#include "base/stl_util-inl.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "net/base/network_change_notifier.h"

ServiceProcess* g_service_process = NULL;

ServiceProcess::ServiceProcess() {
  DCHECK(!g_service_process);
  g_service_process = this;
}

bool ServiceProcess::Initialize() {
  network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  io_thread_.reset(new base::Thread("ServiceProcess_IO"));
  file_thread_.reset(new base::Thread("ServiceProcess_File"));
  if (!io_thread_->StartWithOptions(options) ||
      !file_thread_->StartWithOptions(options)) {
    NOTREACHED();
    Teardown();
    return false;
  }
  return true;
}

bool ServiceProcess::Teardown() {
  io_thread_.reset();
  file_thread_.reset();
  STLDeleteElements(&cloud_print_proxy_list_);
  // The NetworkChangeNotifier must be destroyed after all other threads that
  // might use it have been shut down.
  network_change_notifier_.reset();
  return true;
}

CloudPrintProxy* ServiceProcess::CreateCloudPrintProxy(
    JsonPrefStore* service_prefs) {
  CloudPrintProxy* cloud_print_proxy = new CloudPrintProxy();
  cloud_print_proxy->Initialize(service_prefs);
  cloud_print_proxy_list_.push_back(cloud_print_proxy);
  return cloud_print_proxy;
}

ServiceProcess::~ServiceProcess() {
  Teardown();
  DCHECK(cloud_print_proxy_list_.size() == 0);
  g_service_process = NULL;
}

