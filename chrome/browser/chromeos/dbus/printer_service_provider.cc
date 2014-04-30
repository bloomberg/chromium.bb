// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/printer_service_provider.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "net/base/escape.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/aura/window.h"

namespace {

const char kPrinterAdded[] = "PrinterAdded";

enum PrinterServiceEvent {
  PRINTER_ADDED,
  PAGE_DISPLAYED,
  PRINTER_SERVICE_EVENT_MAX,
};

// TODO(vitalybuka): update URL with more relevant information.
const char kCloudPrintLearnUrl[] =
    "https://www.google.com/landing/cloudprint/index.html";

void ActivateContents(Browser* browser, content::WebContents* contents) {
  browser->tab_strip_model()->ActivateTabAt(
      browser->tab_strip_model()->GetIndexOfWebContents(contents), false);
}

Browser* ActivateAndGetBrowserForUrl(GURL url) {
  for (TabContentsIterator it; !it.done(); it.Next()) {
    if (it->GetLastCommittedURL() == url) {
      ActivateContents(it.browser(), *it);
      return it.browser();
    }
  }
  return NULL;
}

void FindOrOpenCloudPrintPage(const std::string& /* vendor */,
                              const std::string& /* product */) {
  UMA_HISTOGRAM_ENUMERATION("PrinterService.PrinterServiceEvent", PRINTER_ADDED,
                            PRINTER_SERVICE_EVENT_MAX);
  if (!ash::Shell::GetInstance()->session_state_delegate()->
          IsActiveUserSessionStarted() ||
      ash::Shell::GetInstance()->session_state_delegate()->IsScreenLocked()) {
    return;
  }

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return;

  GURL url(kCloudPrintLearnUrl);

  if (!ActivateAndGetBrowserForUrl(url)) {
    chrome::ScopedTabbedBrowserDisplayer displayer(
        profile, chrome::HOST_DESKTOP_TYPE_ASH);
    UMA_HISTOGRAM_ENUMERATION("PrinterService.PrinterServiceEvent",
                              PAGE_DISPLAYED, PRINTER_SERVICE_EVENT_MAX);
    chrome::AddSelectedTabWithURL(displayer.browser(), url,
                                  content::PAGE_TRANSITION_LINK);
  }
}

}  // namespace

namespace chromeos {

PrinterServiceProvider::PrinterServiceProvider()
    : weak_ptr_factory_(this) {
}

PrinterServiceProvider::~PrinterServiceProvider() {
}

void PrinterServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {

  exported_object_ = exported_object;
  DVLOG(1) << "PrinterServiceProvider started";
  exported_object_->ExportMethod(
      kLibCrosServiceInterface,
      kPrinterAdded,
      base::Bind(&PrinterServiceProvider::PrinterAdded,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&PrinterServiceProvider::OnExported,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PrinterServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "."
               << method_name;
  }
  DVLOG(1) << "Method exported: " << interface_name << "." << method_name;
}

void PrinterServiceProvider::ShowCloudPrintHelp(const std::string& vendor,
                                                const std::string& product) {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(&FindOrOpenCloudPrintPage, vendor,
                                              product));
}

void PrinterServiceProvider::PrinterAdded(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(1) << "PrinterAdded " << method_call->ToString();

  dbus::MessageReader reader(method_call);
  std::string vendor;
  std::string product;
  // Don't check for error, parameters are optional. If some string is empty
  // web server will show generic help page.
  reader.PopString(&vendor);
  reader.PopString(&product);
  ShowCloudPrintHelp(vendor, product);

  // Send an empty response.
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

}  // namespace chromeos

