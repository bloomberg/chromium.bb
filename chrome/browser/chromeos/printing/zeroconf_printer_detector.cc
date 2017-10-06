// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/zeroconf_printer_detector.h"

#include <map>
#include <string>
#include <vector>

#include "base/md5.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list_threadsafe.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {
namespace {

using local_discovery::ServiceDescription;
using local_discovery::ServiceDiscoveryDeviceLister;
using local_discovery::ServiceDiscoverySharedClient;

// Supported service names for printers.
const char* kIppServiceName = "_ipp._tcp.local";
const char* kIppsServiceName = "_ipps._tcp.local";

// These (including the default values) come from section 9.2 of the Bonjour
// Printing Spec v1.2, and the field names follow the spec definitions instead
// of the canonical Chromium style.
//
// Not all of these will necessarily be specified for a given printer.  Also, we
// only define the fields that we care about, others not listed here we just
// ignore.
class ParsedMetadata {
 public:
  std::string adminurl;
  std::string air = "none";
  std::string note;
  std::string pdl = "application/postscript";
  // We stray slightly from the spec for product.  In the bonjour spec, product
  // is always enclosed in parentheses because...reasons.  We strip out parens.
  std::string product;
  std::string rp;
  std::string ty;
  std::string usb_MDL;
  std::string usb_MFG;
  std::string UUID;

  // Parse out metadata from sd to fill this structure.
  explicit ParsedMetadata(const ServiceDescription& sd) {
    for (const std::string& m : sd.metadata) {
      size_t equal_pos = m.find('=');
      if (equal_pos == std::string::npos) {
        // Malformed, skip it.
        continue;
      }
      base::StringPiece key(m.data(), equal_pos);
      base::StringPiece value(m.data() + equal_pos + 1,
                              m.length() - (equal_pos + 1));
      if (key == "note") {
        note = value.as_string();
      } else if (key == "pdl") {
        pdl = value.as_string();
      } else if (key == "product") {
        // Strip parens; ignore anything not enclosed in parens as malformed.
        if (value.starts_with("(") && value.ends_with(")")) {
          product = value.substr(1, value.size() - 2).as_string();
        }
      } else if (key == "rp") {
        rp = value.as_string();
      } else if (key == "ty") {
        ty = value.as_string();
      } else if (key == "usb_MDL") {
        usb_MDL = value.as_string();
      } else if (key == "usb_MFG") {
        usb_MFG = value.as_string();
      } else if (key == "UUID") {
        UUID = value.as_string();
      }
    }
  }
  ParsedMetadata(const ParsedMetadata& other) = delete;
};

// Create a unique identifier for this printer based on the ServiceDescription.
// This is what is used to determine whether or not this is the same printer
// when seen again later.  We use an MD5 hash of fields we expect to be
// immutable.
//
// These ids are persistent in synced storage; if you change this function
// carelessly, you will create mismatches between users' stored printer
// configurations and the printers themselves.
//
// Note we explicitly *don't* use the service type in this hash, because the
// same printer may export multiple services (ipp and ipps), and we want them
// all to be considered the same printer.
std::string ZeroconfPrinterId(const ServiceDescription& service,
                              const ParsedMetadata& metadata) {
  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, service.instance_name());
  base::MD5Update(&ctx, metadata.product);
  base::MD5Update(&ctx, metadata.UUID);
  base::MD5Update(&ctx, metadata.usb_MFG);
  base::MD5Update(&ctx, metadata.usb_MDL);
  base::MD5Update(&ctx, metadata.ty);
  base::MD5Update(&ctx, metadata.rp);
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  return base::StringPrintf("zeroconf-%s",
                            base::MD5DigestToBase16(digest).c_str());
}

// Attempt to fill |detected_printer| using the information in
// |service_description| and |metadata|.  Return true on success, false on
// failure.
bool ConvertToPrinter(const ServiceDescription& service_description,
                      const ParsedMetadata& metadata,
                      PrinterDetector::DetectedPrinter* detected_printer) {
  // If we don't have the minimum information needed to attempt a setup, fail.
  // Also fail on a port of 0, as this is used to indicate that the service
  // doesn't *actually* exist, the device just wants to guard the name.
  if (service_description.service_name.empty() || metadata.ty.empty() ||
      service_description.ip_address.empty() ||
      (service_description.address.port() == 0)) {
    return false;
  }

  Printer& printer = detected_printer->printer;
  printer.set_id(ZeroconfPrinterId(service_description, metadata));
  printer.set_uuid(metadata.UUID);
  printer.set_display_name(metadata.ty);
  printer.set_description(metadata.note);
  printer.set_make_and_model(metadata.product);
  const char* uri_protocol;
  if (service_description.service_type() ==
      base::StringPiece(kIppServiceName)) {
    uri_protocol = "ipp";
  } else if (service_description.service_type() ==
             base::StringPiece(kIppsServiceName)) {
    uri_protocol = "ipps";
  } else {
    // Since we only register for these services, we should never get back
    // a service other than the ones above.
    NOTREACHED() << "Zeroconf printer with unknown service type"
                 << service_description.service_type();
    return false;
  }
  printer.set_uri(base::StringPrintf(
      "%s://%s/%s", uri_protocol,
      service_description.address.ToString().c_str(), metadata.rp.c_str()));

  // Use an effective URI with a pre-resolved ip address and port, since CUPS
  // can't resolve these addresses in ChromeOS (crbug/626377).
  printer.set_effective_uri(base::StringPrintf(
      "%s://%s:%d/%s", uri_protocol,
      service_description.ip_address.ToString().c_str(),
      service_description.address.port(), metadata.rp.c_str()));

  // gather ppd identification candidates.
  if (!metadata.ty.empty()) {
    detected_printer->ppd_search_data.make_and_model.push_back(metadata.ty);
  }
  if (!metadata.product.empty()) {
    detected_printer->ppd_search_data.make_and_model.push_back(
        metadata.product);
  }
  if (!metadata.usb_MFG.empty() && !metadata.usb_MDL.empty()) {
    detected_printer->ppd_search_data.make_and_model.push_back(
        base::StringPrintf("%s %s", metadata.usb_MFG.c_str(),
                           metadata.usb_MDL.c_str()));
  }
  return true;
}

// Given two printers which are actually the same printer advertised two
// different ways (e.g. ipp vs ipps), return true if the candidate record
// should replace the existing record.
bool ShouldReplaceRecord(const PrinterDetector::DetectedPrinter& existing,
                         const PrinterDetector::DetectedPrinter& candidate) {
  // Right now the only time this should happen is for ipp vs ipps.
  return (base::StringPiece(existing.printer.uri()).starts_with("ipp://") &&
          base::StringPiece(candidate.printer.uri()).starts_with("ipps://"));
}

class ZeroconfPrinterDetectorImpl
    : public ZeroconfPrinterDetector,
      public ServiceDiscoveryDeviceLister::Delegate {
 public:
  explicit ZeroconfPrinterDetectorImpl(Profile* profile)
      : discovery_client_(ServiceDiscoverySharedClient::GetInstance()),
        observer_list_(new base::ObserverListThreadSafe<Observer>()) {
    std::array<const char*, 2> services{{kIppServiceName, kIppsServiceName}};

    // Since we start the discoverers immediately, this must come last in the
    // constructor.
    for (const char* service : services) {
      device_listers_.emplace_back(
          base::MakeUnique<ServiceDiscoveryDeviceLister>(
              this, discovery_client_.get(), service));
      device_listers_.back()->Start();
      device_listers_.back()->DiscoverNewDevices();
    }
  }

  ~ZeroconfPrinterDetectorImpl() override {}

  std::vector<DetectedPrinter> GetPrinters() override {
    base::AutoLock auto_lock(printers_lock_);
    return GetPrintersLocked();
  }

  void AddObserver(Observer* observer) override {
    observer_list_->AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_->RemoveObserver(observer);
  }

  // Schedules callbacks for the observers using ThreadSafeObserverList. This
  // means that the callbacks are posted for later execution and not executed
  // immediately.
  void StartObservers() override {
    observer_list_->Notify(
        FROM_HERE, &PrinterDetector::Observer::OnPrintersFound, GetPrinters());
    // TODO(justincarlson) - Figure out a more intelligent way to figure out
    // when a scan is reasonably "done".
    observer_list_->Notify(FROM_HERE,
                           &PrinterDetector::Observer::OnPrinterScanComplete);
  }

  // ServiceDiscoveryDeviceLister::Delegate implementation
  void OnDeviceChanged(bool added,
                       const ServiceDescription& service_description) override {
    // We don't care if it was added or not; we generate an update either way.
    ParsedMetadata metadata(service_description);
    DetectedPrinter printer;
    if (!ConvertToPrinter(service_description, metadata, &printer)) {
      return;
    }
    base::AutoLock auto_lock(printers_lock_);

    auto existing = printers_.find(service_description.instance_name());
    if (existing == printers_.end() ||
        ShouldReplaceRecord(existing->second, printer)) {
      printers_[service_description.instance_name()] = printer;
      observer_list_->Notify(FROM_HERE,
                             &PrinterDetector::Observer::OnPrintersFound,
                             GetPrintersLocked());
    }
  }

  void OnDeviceRemoved(const std::string& service_name) override {
    // Leverage ServiceDescription parsing to pull out the instance name.
    ServiceDescription service_description;
    service_description.service_name = service_name;
    base::AutoLock auto_lock(printers_lock_);
    auto it = printers_.find(service_description.instance_name());
    if (it != printers_.end()) {
      printers_.erase(it);
      observer_list_->Notify(FROM_HERE,
                             &PrinterDetector::Observer::OnPrintersFound,
                             GetPrintersLocked());
    }
  }

  // Don't need to do anything here.
  void OnDeviceCacheFlushed() override {}

 private:
  // Requires that printers_lock_ be held.
  std::vector<DetectedPrinter> GetPrintersLocked() {
    printers_lock_.AssertAcquired();
    std::vector<DetectedPrinter> ret;
    ret.reserve(printers_.size());
    for (const auto& entry : printers_) {
      ret.push_back(entry.second);
    }
    return ret;
  }

  // Map from service name to associated known printer, and associated lock.
  std::map<std::string, DetectedPrinter> printers_;
  base::Lock printers_lock_;

  // Keep a reference to the shared device client around for the lifetime of
  // this object.
  scoped_refptr<ServiceDiscoverySharedClient> discovery_client_;
  std::vector<std::unique_ptr<ServiceDiscoveryDeviceLister>> device_listers_;

  // Observers of this object.
  scoped_refptr<base::ObserverListThreadSafe<Observer>> observer_list_;
};

}  // namespace

// static
//
std::unique_ptr<ZeroconfPrinterDetector> ZeroconfPrinterDetector::Create(
    Profile* profile) {
  return base::MakeUnique<ZeroconfPrinterDetectorImpl>(profile);
}

}  // namespace chromeos
