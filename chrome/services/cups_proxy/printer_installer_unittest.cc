// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cups/cups.h>

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/services/cups_proxy/fake_cups_proxy_service_delegate.h"
#include "chrome/services/cups_proxy/printer_installer.h"
#include "printing/backend/cups_ipp_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cups_proxy {
namespace {

using Printer = chromeos::Printer;

// Generated via base::GenerateGUID.
const char kGenericGUID[] = "fd4c5f2e-7549-43d5-b931-9bf4e4f1bf51";

// Faked delegate gives control over PrinterInstaller's printing stack
// dependencies.
class FakeServiceDelegate
    : public chromeos::printing::FakeCupsProxyServiceDelegate {
 public:
  FakeServiceDelegate() = default;
  ~FakeServiceDelegate() override = default;

  void AddPrinter(const Printer& printer) {
    installed_printers_.insert({printer.id(), false});
  }

  // Service delegate overrides.
  bool IsPrinterInstalled(const Printer& printer) override {
    if (!base::ContainsKey(installed_printers_, printer.id())) {
      return false;
    }

    return installed_printers_.at(printer.id());
  }

  base::Optional<Printer> GetPrinter(const std::string& id) override {
    if (!base::ContainsKey(installed_printers_, id)) {
      return base::nullopt;
    }

    return Printer(id);
  }

  void SetupPrinter(
      const Printer& printer,
      chromeos::printing::PrinterSetupCallback callback) override {
    // PrinterInstaller is expected to have checked if |printer| is already
    // installed before trying setup.
    if (IsPrinterInstalled(printer)) {
      return std::move(callback).Run(false);
    }

    // Install printer.
    installed_printers_[printer.id()] = true;
    return std::move(callback).Run(true);
  }

 private:
  std::map<std::string, bool> installed_printers_;
};

class PrinterInstallerTest : public testing::Test {
 public:
  PrinterInstallerTest() : weak_factory_(this) {
    delegate_ = std::make_unique<FakeServiceDelegate>();
    printer_installer_ = PrinterInstaller::Create(delegate_->GetWeakPtr());
  }

  ~PrinterInstallerTest() override = default;

  bool RunInstallPrinterIfNeeded(ipp_t* ipp, Printer to_install) {
    bool success = false;
    printer_installer_->InstallPrinterIfNeeded(
        ipp, base::BindOnce(&PrinterInstallerTest::OnRunInstallPrinterIfNeeded,
                            weak_factory_.GetWeakPtr(), &success,
                            std::move(to_install)));
    scoped_task_environment_.RunUntilIdle();
    return success;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  void OnRunInstallPrinterIfNeeded(bool* ret,
                                   Printer to_install,
                                   InstallPrinterResult result) {
    if (result != InstallPrinterResult::kSuccess) {
      return;
    }

    // If printer wasn't installed, fail.
    if (!delegate_->IsPrinterInstalled(to_install)) {
      return;
    }

    *ret = true;
  }

  // Backend fake driving the PrinterInstaller.
  std::unique_ptr<FakeServiceDelegate> delegate_;

  // The class being tested. This must be declared after the fakes, as its
  // initialization must come after that of the fakes.
  std::unique_ptr<PrinterInstaller> printer_installer_;

  base::WeakPtrFactory<PrinterInstallerTest> weak_factory_;
};

// Return a valid ScopedIppPtr that correctly references |id| in
// the printer-uri field.
::printing::ScopedIppPtr MakeIppReferencingPrinters(const std::string& id) {
  ::printing::ScopedIppPtr ret = ::printing::WrapIpp(ippNew());

  std::string uri = "ipp://localhost/printers/" + id;
  ippAddString(ret.get(), IPP_TAG_PRINTER, IPP_TAG_URI, "printer-uri", NULL,
               uri.c_str());

  return ret;
}

// Standard install known printer workflow.
TEST_F(PrinterInstallerTest, SimpleSanityTest) {
  Printer to_install(kGenericGUID);
  delegate_->AddPrinter(to_install);

  auto ipp = MakeIppReferencingPrinters(to_install.id());
  EXPECT_TRUE(RunInstallPrinterIfNeeded(ipp.get(), std::move(to_install)));
}

// Should fail to install an unknown(previously unseen) printer.
TEST_F(PrinterInstallerTest, UnknownPrinter) {
  Printer to_install(kGenericGUID);

  auto ipp = MakeIppReferencingPrinters(to_install.id());
  EXPECT_FALSE(RunInstallPrinterIfNeeded(ipp.get(), std::move(to_install)));
}

// Ensure we never setup a printer that's already installed.
TEST_F(PrinterInstallerTest, InstallPrinterTwice) {
  Printer to_install(kGenericGUID);
  delegate_->AddPrinter(to_install);

  auto ipp = MakeIppReferencingPrinters(to_install.id());
  EXPECT_TRUE(RunInstallPrinterIfNeeded(ipp.get(), to_install));

  // |printer_installer_| should notice printer is already installed and bail
  // out. If it attempts setup, FakeServiceDelegate will fail the request.
  EXPECT_TRUE(RunInstallPrinterIfNeeded(ipp.get(), to_install));
}

}  // namespace
}  // namespace cups_proxy
