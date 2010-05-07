// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_BACKEND_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_BACKEND_H_

#include <map>
#include <string>

#include "base/thread.h"
#include "chrome/browser/printing/cloud_print/printer_info.h"
#include "chrome/common/net/url_fetcher.h"

class CloudPrintProxyService;
class DictionaryValue;

// CloudPrintProxyFrontend is the interface used by CloudPrintProxyBackend to
// communicate with the entity that created it and, presumably, is interested in
// cloud print proxy related activity.
// NOTE: All methods will be invoked by a CloudPrintProxyBackend on the same
// thread used to create that CloudPrintProxyBackend.
class CloudPrintProxyFrontend {
 public:
  CloudPrintProxyFrontend() {}

  // There is a list of printers available that can be registered.
  virtual void OnPrinterListAvailable(
      const cloud_print::PrinterList& printer_list) = 0;

 protected:
  // Don't delete through SyncFrontend interface.
  virtual ~CloudPrintProxyFrontend() {
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxyFrontend);
};

class CloudPrintProxyBackend {
 public:
  explicit CloudPrintProxyBackend(CloudPrintProxyFrontend* frontend);
  ~CloudPrintProxyBackend();

  bool Initialize(const std::string& auth_token, const std::string& proxy_id);
  void Shutdown();
  void RegisterPrinters(const cloud_print::PrinterList& printer_list);
  void HandlePrinterNotification(const std::string& printer_id);

 private:
  // The real guts of SyncBackendHost, to keep the public client API clean.
  class Core;
  // A thread we dedicate for use to perform initialization and
  // authentication.
  base::Thread core_thread_;
  // Our core, which communicates with AuthWatcher for GAIA authentication and
  // which contains printer registration code.
  scoped_refptr<Core> core_;
  // A reference to the MessageLoop used to construct |this|, so we know how
  // to safely talk back to the SyncFrontend.
  MessageLoop* const frontend_loop_;
  // The frontend which is responsible for displaying UI and updating Prefs
  CloudPrintProxyFrontend* frontend_;

  friend class base::RefCountedThreadSafe<CloudPrintProxyBackend::Core>;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxyBackend);
};

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_BACKEND_H_

