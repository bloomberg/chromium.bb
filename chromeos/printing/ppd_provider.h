// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PPD_PROVIDER_H_
#define CHROMEOS_PRINTING_PPD_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/printing/printer_configuration.h"

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {
namespace printing {

class PpdCache;

// PpdProvider is responsible for mapping printer descriptions to
// CUPS-PostScript Printer Description (PPD) files.  It provides PPDs that a
// user previously identified for use, and falls back to querying quirksserver
// based on manufacturer/model of the printer.
//
// All functions in this class must be called from a sequenced context.
class CHROMEOS_EXPORT PpdProvider : public base::RefCounted<PpdProvider> {
 public:
  // Possible result codes of a Resolve*() call.
  enum CallbackResultCode {
    SUCCESS,

    // Looked for a PPD for this configuration, but couldn't find a match.
    // Never returned for QueryAvailable().
    NOT_FOUND,

    // Failed to contact an external server needed to finish resolution.
    SERVER_ERROR,

    // Other error that is not expected to be transient.
    INTERNAL_ERROR,
  };

  // Construction-time options.  Everything in this structure should have
  // a sane default.
  struct Options {
    Options() {}

    // Root of the ppd serving hierarchy.
    std::string ppd_server_root = "https://www.gstatic.com/chromeos_printing";
  };

  // Result of a ResolvePpd() call.  If the result code is SUCCESS, then the
  // string holds the contents of a PPD (that may or may not be gzipped).
  // Otherwise, the string will be empty.
  using ResolvePpdCallback =
      base::Callback<void(CallbackResultCode, const std::string&)>;

  // Result of a ResolveManufacturers() call.  If the result code is SUCCESS,
  // then the vector contains a sorted list of manufacturers for which we have
  // at least one printer driver.
  using ResolveManufacturersCallback =
      base::Callback<void(CallbackResultCode, const std::vector<std::string>&)>;

  // Result of a ResolvePrinters() call.  If the result code is SUCCESS, then
  // the vector contains a sorted list of all printer models from the given
  // manufacturer for which we have a driver.
  using ResolvePrintersCallback =
      base::Callback<void(CallbackResultCode, const std::vector<std::string>&)>;

  // Result of a ResolveUsbIds call.  If the result code is SUCCESS, then the
  // second argument contains the effective make and model of the printer.
  // NOT_FOUND means we don't know about this Usb device.
  using ResolveUsbIdsCallback =
      base::Callback<void(CallbackResultCode, const std::string&)>;

  // Create and return a new PpdProvider with the given cache and options.
  // A references to |url_context_getter| is taken.
  static scoped_refptr<PpdProvider> Create(
      const std::string& browser_locale,
      scoped_refptr<net::URLRequestContextGetter> url_context_getter,
      scoped_refptr<PpdCache> cache,
      scoped_refptr<base::SequencedTaskRunner> disk_task_runner,
      const Options& options = Options());

  // Get all manufacturers for which we have drivers.  Keys of the map will be
  // localized in the default browser locale or the closest available fallback.
  //
  // |cb| will be called on the invoking thread, and will be sequenced.
  virtual void ResolveManufacturers(const ResolveManufacturersCallback& cb) = 0;

  // Get all models from a given manufacturer, localized in the default browser
  // locale or the closest available fallback.  |manufacturer| must be a value
  // returned from a successful ResolveManufacturers() call performed from this
  // PpdProvider instance.
  //
  // |cb| will be called on the invoking thread, and will be sequenced.
  virtual void ResolvePrinters(const std::string& manufacturer,
                               const ResolvePrintersCallback& cb) = 0;

  // Given a usb vendor/device id, attempt to get an effective make and model
  // string for the given printer.
  virtual void ResolveUsbIds(int vendor_id,
                             int device_id,
                             const ResolveUsbIdsCallback& cb) = 0;

  // Given a |manufacturer| from ResolveManufacturers() and a |printer| from
  // a ResolvePrinters() call for that manufacturer, fill in |reference|
  // with the information needed to resolve the Ppd for this printer.  Returns
  // true on success and overwrites the contents of |reference|.  On failure,
  // |reference| is unchanged.
  //
  // Note that, unlike the other functions in this class, |reference| can be
  // saved and given to ResolvePpd() in a different PpdProvider instance without
  // first resolving manufactuerers or printers.
  virtual bool GetPpdReference(const std::string& manufacturer,
                               const std::string& printer,
                               Printer::PpdReference* reference) const = 0;

  // Given a PpdReference, attempt to get the PPD for printing.
  //
  // |cb| will be called on the invoking thread, and will be sequenced.
  virtual void ResolvePpd(const Printer::PpdReference& reference,
                          const ResolvePpdCallback& cb) = 0;

  // Hook for testing.  Returns true if there are no API calls that have not
  // yet completed.
  virtual bool Idle() const = 0;

 protected:
  friend class base::RefCounted<PpdProvider>;
  virtual ~PpdProvider() {}
};

}  // namespace printing
}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PPD_PROVIDER_H_
