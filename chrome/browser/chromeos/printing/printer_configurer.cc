// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printer_configurer.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/component_updater/cros_component_installer.h"
#include "chrome/browser/local_discovery/endpoint_resolver.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

const std::map<const std::string, const std::string>&
GetComponentizedFilters() {
  // A mapping from filter names to available components for downloads.
  static const auto* const componentized_filters =
      new std::map<const std::string, const std::string>{
          {"epson-escpr-wrapper", "epson-inkjet-printer-escpr"},
          {"epson-escpr", "epson-inkjet-printer-escpr"},
          {"rastertostar", "star-cups-driver"},
          {"rastertostarlm", "star-cups-driver"}};
  return *componentized_filters;
}

namespace chromeos {

namespace {

// Get the URI that we want for talking to cups.
std::string URIForCups(const Printer& printer) {
  if (!printer.effective_uri().empty()) {
    return printer.effective_uri();
  } else {
    return printer.uri();
  }
}

// Configures printers by downloading PPDs then adding them to CUPS through
// debugd.  This class must be used on the UI thread.
class PrinterConfigurerImpl : public PrinterConfigurer {
 public:
  explicit PrinterConfigurerImpl(Profile* profile)
      : endpoint_resolver_(new local_discovery::EndpointResolver()),
        ppd_provider_(CreatePpdProvider(profile)),
        weak_factory_(this) {}

  PrinterConfigurerImpl(const PrinterConfigurerImpl&) = delete;
  PrinterConfigurerImpl& operator=(const PrinterConfigurerImpl&) = delete;

  ~PrinterConfigurerImpl() override {}

  void SetUpPrinter(const Printer& printer,
                    PrinterSetupCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!printer.id().empty());
    DCHECK(!printer.uri().empty());

    if (!printer.RequiresIpResolution()) {
      StartConfiguration(printer, std::move(callback));
      return;
    }

    // Resolve the uri to an ip with a mutable copy of the printer.
    endpoint_resolver_->Start(
        printer.GetHostAndPort(),
        base::BindOnce(
            &PrinterConfigurerImpl::OnIpResolved, weak_factory_.GetWeakPtr(),
            std::make_unique<Printer>(printer), std::move(callback)));
  }

 private:
  // Run installation for a printer with a resolved uri.  |callback| is called
  // with the result of the setup when it is complete.
  void StartConfiguration(const Printer& printer,
                          PrinterSetupCallback callback) {
    if (!printer.IsIppEverywhere()) {
      ppd_provider_->ResolvePpd(
          printer.ppd_reference(),
          base::BindOnce(&PrinterConfigurerImpl::ResolvePpdDone,
                         weak_factory_.GetWeakPtr(), printer,
                         std::move(callback)));
      return;
    }

    auto* client = DBusThreadManager::Get()->GetDebugDaemonClient();
    client->CupsAddAutoConfiguredPrinter(
        printer.id(), URIForCups(printer),
        base::BindOnce(&PrinterConfigurerImpl::OnAddedPrinter,
                       weak_factory_.GetWeakPtr(), printer,
                       std::move(callback)));
  }

  // Callback for when the IP for a zeroconf printer has been resolved.  If the
  // request was successful, sets the |effective_uri| on |printer| with
  // |endpoint| then continues setup. |cb| is called with a result reporting the
  // success or failure of the setup operation, eventually.
  void OnIpResolved(std::unique_ptr<Printer> printer,
                    PrinterSetupCallback cb,
                    const net::IPEndPoint& endpoint) {
    if (!endpoint.address().IsValid()) {
      // |endpoint| does not have a valid address. Address was not resolved.
      std::move(cb).Run(kPrinterUnreachable);
      return;
    }

    std::string effective_uri = printer->ReplaceHostAndPort(endpoint);
    printer->set_effective_uri(effective_uri);

    StartConfiguration(*printer, std::move(cb));
  }

  void OnAddedPrinter(const Printer& printer,
                      PrinterSetupCallback cb,
                      base::Optional<int32_t> result_code) {
    // It's expected that debug daemon posts callbacks on the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (!result_code.has_value()) {
      LOG(WARNING) << "Could not contact debugd";
      std::move(cb).Run(PrinterSetupResult::kDbusError);
      return;
    }

    PrinterSetupResult result;
    switch (result_code.value()) {
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
        LOG(ERROR) << "Unrecognized printer setup error: "
                   << result_code.value();
        result = PrinterSetupResult::kFatalError;
        break;
    }

    std::move(cb).Run(result);
  }

  void AddPrinter(const Printer& printer,
                  const std::string& ppd_contents,
                  PrinterSetupCallback cb) {
    auto* client = DBusThreadManager::Get()->GetDebugDaemonClient();

    client->CupsAddManuallyConfiguredPrinter(
        printer.id(), URIForCups(printer), ppd_contents,
        base::BindOnce(&PrinterConfigurerImpl::OnAddedPrinter,
                       weak_factory_.GetWeakPtr(), printer, std::move(cb)));
  }

  // Executed on component load API finish.
  // Check API return result to decide whether component is successfully loaded.
  void OnComponentLoad(const Printer& printer,
                       const std::string& ppd_contents,
                       PrinterSetupCallback cb,
                       const base::FilePath& result) {
    // Result is the component mount point, or empty
    // if the component couldn't be loaded
    if (result.empty()) {
      LOG(ERROR) << "Filter component installation fails.";
      std::move(cb).Run(PrinterSetupResult::kFatalError);
    } else {
      AddPrinter(printer, ppd_contents, std::move(cb));
    }
  }

  void ResolvePpdSuccess(const Printer& printer,
                         PrinterSetupCallback cb,
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
        g_browser_process->platform_part()->cros_component_manager()->Load(
            component_name,
            base::BindOnce(&PrinterConfigurerImpl::OnComponentLoad,
                           weak_factory_.GetWeakPtr(), printer, ppd_contents,
                           std::move(cb)));
        return;
      } else if (components_requested.size() > 1) {
        LOG(ERROR) << "More than one filter component is requested.";
        std::move(cb).Run(PrinterSetupResult::kFatalError);
        return;
      }
    }
    AddPrinter(printer, ppd_contents, std::move(cb));
  }

  void ResolvePpdDone(const Printer& printer,
                      PrinterSetupCallback cb,
                      PpdProvider::CallbackResultCode result,
                      const std::string& ppd_contents,
                      const std::vector<std::string>& ppd_filters) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    switch (result) {
      case PpdProvider::SUCCESS:
        DCHECK(!ppd_contents.empty());
        ResolvePpdSuccess(printer, std::move(cb), ppd_contents, ppd_filters);
        break;
      case PpdProvider::CallbackResultCode::NOT_FOUND:
        std::move(cb).Run(PrinterSetupResult::kPpdNotFound);
        break;
      case PpdProvider::CallbackResultCode::SERVER_ERROR:
        std::move(cb).Run(PrinterSetupResult::kPpdUnretrievable);
        break;
      case PpdProvider::CallbackResultCode::INTERNAL_ERROR:
        std::move(cb).Run(PrinterSetupResult::kFatalError);
        break;
      case PpdProvider::CallbackResultCode::PPD_TOO_LARGE:
        std::move(cb).Run(PrinterSetupResult::kPpdTooLarge);
        break;
    }
  }

  std::unique_ptr<local_discovery::EndpointResolver> endpoint_resolver_;
  scoped_refptr<PpdProvider> ppd_provider_;
  base::WeakPtrFactory<PrinterConfigurerImpl> weak_factory_;
};

}  // namespace

// static
std::string PrinterConfigurer::SetupFingerprint(const Printer& printer) {
  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, printer.id());
  base::MD5Update(&ctx, URIForCups(printer));
  base::MD5Update(&ctx, printer.ppd_reference().user_supplied_ppd_url);
  base::MD5Update(&ctx, printer.ppd_reference().effective_make_and_model);
  char autoconf = printer.ppd_reference().autoconf ? 1 : 0;
  base::MD5Update(&ctx, std::string(&autoconf, sizeof(autoconf)));
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  return std::string(reinterpret_cast<char*>(&digest.a[0]), sizeof(digest.a));
}

// static
std::unique_ptr<PrinterConfigurer> PrinterConfigurer::Create(Profile* profile) {
  return base::MakeUnique<PrinterConfigurerImpl>(profile);
}

}  // namespace chromeos
