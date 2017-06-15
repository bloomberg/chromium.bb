// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_configurer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/component_updater/cros_component_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

const std::map<const std::string, const std::string>&
GetComponentizedFilters() {
  // A mapping from filter names to available components for downloads.
  static const auto* const componentized_filters =
      new std::map<const std::string, const std::string>{
          {"epson-escpr-wrapper", "epson-inkjet-printer-escpr"}};
  return *componentized_filters;
}

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

    PrinterSetupResult result;
    switch (result_code) {
      case debugd::CupsResult::CUPS_SUCCESS:
        result = PrinterSetupResult::kSuccess;
        break;
      case debugd::CupsResult::CUPS_INVALID_PPD:
        result = PrinterSetupResult::kInvalidPpd;
        break;
      case debugd::CupsResult::CUPS_AUTOCONF_FAILURE:
        // There are other reasons autoconf fails but this is the most likely.
        result = PrinterSetupResult::kPrinterUnreachable;
        break;
      case debugd::CupsResult::CUPS_LPADMIN_FAILURE:
        // Printers should always be configurable by lpadmin.
        NOTREACHED() << "lpadmin could not add the printer";
        result = PrinterSetupResult::kFatalError;
        break;
      case debugd::CupsResult::CUPS_FATAL:
      default:
        // We have no idea.  It must be fatal.
        LOG(ERROR) << "Unrecognized printer setup error: " << result_code;
        result = PrinterSetupResult::kFatalError;
        break;
    }

    cb.Run(result);
  }

  void OnDbusError(const PrinterSetupCallback& cb) {
    // The callback is expected to run on the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    LOG(WARNING) << "Could not contact debugd";
    cb.Run(PrinterSetupResult::kDbusError);
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

  // Executed on component load API finish.
  // Check API return result to decide whether component is successfully loaded.
  void OnComponentLoad(const Printer& printer,
                       const std::string& ppd_contents,
                       const PrinterSetupCallback& cb,
                       const std::string& result) {
    // Result is the component mount point, or empty
    // if the component couldn't be loaded
    if (result.empty()) {
      LOG(ERROR) << "Filter component installation fails.";
      cb.Run(PrinterSetupResult::kFatalError);
    } else {
      AddPrinter(printer, ppd_contents, cb);
    }
  }

  // Returns true if component is requred (and install if possible),
  // Otherwise do nothing and return false.
  bool RequiresComponent(const Printer& printer,
                         const PrinterSetupCallback& cb,
                         const std::string& ppd_contents,
                         const std::vector<std::string>& ppd_filters) {
    if (base::FeatureList::IsEnabled(features::kCrOSComponent)) {
      std::set<std::string> components_requested;
      for (auto& ppd_filter : ppd_filters) {
        for (auto& component : GetComponentizedFilters()) {
          if (component.first == ppd_filter) {
            components_requested.insert(component.second);
          }
        }
      }
      if (components_requested.size() == 1) {
        // Only allow one filter request in ppd file.
        auto& component_name = *components_requested.begin();
        component_updater::CrOSComponent::LoadComponent(
            component_name,
            base::Bind(&PrinterConfigurerImpl::OnComponentLoad,
                       weak_factory_.GetWeakPtr(), printer, ppd_contents, cb));
        return true;
      } else if (components_requested.size() > 1) {
        LOG(ERROR) << "More than one filter components are requested.";
        cb.Run(PrinterSetupResult::kFatalError);
        return true;
      }
    }
    return false;
  }

  void ResolvePpdDone(const Printer& printer,
                      const PrinterSetupCallback& cb,
                      printing::PpdProvider::CallbackResultCode result,
                      const std::string& ppd_contents,
                      const std::vector<std::string>& ppd_filters) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    switch (result) {
      case chromeos::printing::PpdProvider::SUCCESS:
        DCHECK(!ppd_contents.empty());
        if (!RequiresComponent(printer, cb, ppd_contents, ppd_filters)) {
          AddPrinter(printer, ppd_contents, cb);
        }
        break;
      case printing::PpdProvider::CallbackResultCode::NOT_FOUND:
        cb.Run(PrinterSetupResult::kPpdNotFound);
        break;
      case printing::PpdProvider::CallbackResultCode::SERVER_ERROR:
        cb.Run(PrinterSetupResult::kPpdUnretrievable);
        break;
      case printing::PpdProvider::CallbackResultCode::INTERNAL_ERROR:
        // TODO(skau): Add kPpdTooLarge when it's reported by the PpdProvider.
        cb.Run(PrinterSetupResult::kFatalError);
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
