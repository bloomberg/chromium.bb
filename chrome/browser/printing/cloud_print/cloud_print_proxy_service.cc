// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"

#include <stack>
#include <vector>

#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/browser/pref_service.h"

CloudPrintProxyService::CloudPrintProxyService(Profile* profile) {
}

CloudPrintProxyService::~CloudPrintProxyService() {
  Shutdown();
}

void CloudPrintProxyService::Initialize() {
}


void CloudPrintProxyService::EnableForUser(const std::string& auth_token) {
  // TODO(sanjeevr): Add code to communicate with the cloud print proxy code
  // running in the service process here.
}

void CloudPrintProxyService::DisableForUser() {
  Shutdown();
}

void CloudPrintProxyService::Shutdown() {
  // TODO(sanjeevr): Add code to communicate with the cloud print proxy code
  // running in the service process here.
}

