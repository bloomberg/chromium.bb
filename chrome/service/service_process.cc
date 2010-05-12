// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_process.h"

#include "chrome/service/cloud_print/cloud_print_proxy.h"

ServiceProcess* g_service_process = NULL;

ServiceProcess::ServiceProcess() {
  DCHECK(!g_service_process);
  g_service_process = this;
}

bool ServiceProcess::Initialize() {
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
  return true;
}

CloudPrintProxy* ServiceProcess::cloud_print_proxy() {
  if (!cloud_print_proxy_.get()) {
    cloud_print_proxy_.reset(new CloudPrintProxy());
    cloud_print_proxy_->Initialize();
  }
  return cloud_print_proxy_.get();
}

ServiceProcess::~ServiceProcess() {
  Teardown();
  g_service_process = NULL;
}

