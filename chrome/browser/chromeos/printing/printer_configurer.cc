// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_configurer.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

// Configures printers by downloading PPDs then adding them to CUPS through
// debugd.  This class must be used on the UI thread.
class PrinterConfigurerImpl : public PrinterConfigurer {
 public:
  explicit PrinterConfigurerImpl(Profile* profile)
      : ppd_provider_(printing::CreateProvider(profile)), weak_factory_(this) {}

  PrinterConfigurerImpl(const PrinterConfigurerImpl&) = delete;
  PrinterConfigurerImpl& operator=(const PrinterConfigurerImpl&) = delete;

  ~PrinterConfigurerImpl() override {}

  void SetUpPrinter(const Printer& printer,
                    const PrinterSetupCallback& callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!printer.id().empty());
    DCHECK(!printer.uri().empty());

    if (!printer.IsIppEverywhere()) {
      ppd_provider_->ResolvePpd(
          printer.ppd_reference(),
          base::Bind(&PrinterConfigurerImpl::ResolvePpdDone,
                     weak_factory_.GetWeakPtr(), printer, callback));
      return;
    }

    auto* client = chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();

    client->CupsAddAutoConfiguredPrinter(
        printer.id(), printer.uri(),
        base::Bind(&PrinterConfigurerImpl::OnAddedPrinter,
                   weak_factory_.GetWeakPtr(), printer, callback),
        base::Bind(&PrinterConfigurerImpl::OnDbusError,
                   weak_factory_.GetWeakPtr(), callback));
  }

 private:
  void OnAddedPrinter(const Printer& printer,
                      const PrinterSetupCallback& cb,
                      int32_t result_code) {
    // It's expected that debug daemon posts callbacks on the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    PrinterSetupResult result = UNKNOWN;
    switch (result_code) {
      case 0:
        result = SUCCESS;
        break;
      default:
        // TODO(skau): Fill out with more granular errors.
        result = FATAL_ERROR;
        break;
    }

    cb.Run(result);
  }

  void OnDbusError(const PrinterSetupCallback& cb) {
    // The callback is expected to run on the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    LOG(WARNING) << "Could not contact debugd";
    cb.Run(PrinterSetupResult::DBUS_ERROR);
  }

  void AddPrinter(const Printer& printer,
                  const std::string& ppd_contents,
                  const PrinterSetupCallback& cb) {
    auto* client = chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();

    client->CupsAddManuallyConfiguredPrinter(
        printer.id(), printer.uri(), ppd_contents,
        base::Bind(&PrinterConfigurerImpl::OnAddedPrinter,
                   weak_factory_.GetWeakPtr(), printer, cb),
        base::Bind(&PrinterConfigurerImpl::OnDbusError,
                   weak_factory_.GetWeakPtr(), cb));
  }

  void ResolvePpdDone(const Printer& printer,
                      const PrinterSetupCallback& cb,
                      printing::PpdProvider::CallbackResultCode result,
                      const std::string& ppd_contents) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    switch (result) {
      case chromeos::printing::PpdProvider::SUCCESS:
        DCHECK(!ppd_contents.empty());
        AddPrinter(printer, ppd_contents, cb);
        break;
      case printing::PpdProvider::CallbackResultCode::NOT_FOUND:
        cb.Run(PPD_NOT_FOUND);
        break;
      case printing::PpdProvider::CallbackResultCode::SERVER_ERROR:
        cb.Run(PPD_UNRETRIEVABLE);
        break;
      case printing::PpdProvider::CallbackResultCode::INTERNAL_ERROR:
        cb.Run(FATAL_ERROR);
        break;
    }
  }

  scoped_refptr<printing::PpdProvider> ppd_provider_;
  base::WeakPtrFactory<PrinterConfigurerImpl> weak_factory_;
};

}  // namespace

// static
std::unique_ptr<PrinterConfigurer> PrinterConfigurer::Create(Profile* profile) {
  return base::MakeUnique<PrinterConfigurerImpl>(profile);
}

}  // namespace chromeos
