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
  if (backend_.get())
    return;

  backend_.reset(new CloudPrintProxyBackend(this));
  backend_->Initialize(lsid, proxy_id);
}

void CloudPrintProxy::DisableForUser() {
  Shutdown();
}

void CloudPrintProxy::HandlePrinterNotification(
    const std::string& printer_id) {
  if (backend_.get())
    backend_->HandlePrinterNotification(printer_id);
}

void CloudPrintProxy::Shutdown() {
  if (backend_.get())
    backend_->Shutdown();
  backend_.reset();
}

// Notification methods from the backend. Called on UI thread.
void CloudPrintProxy::OnPrinterListAvailable(
    const cloud_print::PrinterList& printer_list) {
  // Here we will trim the list to eliminate printers already registered.
  // If there are any more printers left in the list after trimming, we will
  // show the print selection UI. Any printers left in the list after the user
  // selection process will then be registered.
  backend_->RegisterPrinters(printer_list);
}

