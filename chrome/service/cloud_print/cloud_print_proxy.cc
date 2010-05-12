// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_proxy.h"

CloudPrintProxy::CloudPrintProxy() {
}

CloudPrintProxy::~CloudPrintProxy() {
  Shutdown();
}

void CloudPrintProxy::Initialize() {
}


void CloudPrintProxy::EnableForUser(const std::string& lsid,
                                    const std::string& proxy_id) {
}

void CloudPrintProxy::DisableForUser() {
  Shutdown();
}

void CloudPrintProxy::HandlePrinterNotification(
    const std::string& printer_id) {
}

void CloudPrintProxy::Shutdown() {
}

