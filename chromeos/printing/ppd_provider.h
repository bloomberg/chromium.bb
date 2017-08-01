// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PPD_PROVIDER_H_
#define CHROMEOS_PRINTING_PPD_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/printing/printer_configuration.h"

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

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

  // Everything we might know about a printer when looking for a
  // driver for it.  All of the default values for fields in this struct
  // mean we *don't* have that piece of information.
  //
  // Fields are listed in search order preference -- we use earlier
  // fields first to attempt to find a match.
  struct PrinterSearchData {
    PrinterSearchData();
    PrinterSearchData(const PrinterSearchData& other);
    ~PrinterSearchData();

    // Make-and-model string guesses.
    std::vector<std::string> make_and_model;

    // 16-bit usb identifiers.
    int usb_vendor_id = 0;
    int usb_product_id = 0;
  };

  // Result of a ResolvePpd() call.
  // If the result code is SUCCESS, then:
  //    string holds the contents of a PPD (that may or may not be gzipped).
  //    required_filters holds the names of the filters referenced in the ppd.
  // Otherwise, these fields will be empty.
  using ResolvePpdCallback =
      base::Callback<void(CallbackResultCode,
                          const std::string&,
                          const std::vector<std::string>& required_filters)>;

  // Result of a ResolveManufacturers() call.  If the result code is SUCCESS,
  // then the vector contains a sorted list of manufacturers for which we have
  // at least one printer driver.
  using ResolveManufacturersCallback =
      base::Callback<void(CallbackResultCode, const std::vector<std::string>&)>;

  // A list of printer names paired with the PpdReference that should be used
  // for that printer.
  using ResolvedPrintersList =
      std::vector<std::pair<std::string, Printer::PpdReference>>;

  // Result of a ResolvePrinters() call.  If the result code is SUCCESS, then
  // the vector contains a sorted list <model_name, PpdReference> tuples of all
  // printer models from the given manufacturer for which we have a driver,
  // sorted by model_name.
  using ResolvePrintersCallback =
      base::Callback<void(CallbackResultCode, const ResolvedPrintersList&)>;

  // Result of a ResolvePpdReference call.  If the result code is
  // SUCCESS, then the second argument contains the a PpdReference
  // that we have high confidence can be used to obtain a driver for
  // the printer.  NOT_FOUND means we couldn't confidently figure out
  // a driver for the printer.
  using ResolvePpdReferenceCallback =
      base::Callback<void(CallbackResultCode, const Printer::PpdReference&)>;

  // Result of a ReverseLookup call.  If the result code is SUCCESS, then
  // |manufactuer| and |model| contain the strings that could have generated
  // the reference being looked up.
  using ReverseLookupCallback =
      base::Callback<void(CallbackResultCode,
                          const std::string& manufacturer,
                          const std::string& model)>;

  // Create and return a new PpdProvider with the given cache and options.
  // A references to |url_context_getter| is taken.
  static scoped_refptr<PpdProvider> Create(
      const std::string& browser_locale,
      scoped_refptr<net::URLRequestContextGetter> url_context_getter,
      scoped_refptr<PpdCache> cache,
      const Options& options = Options());

  // Get all manufacturers for which we have drivers.  Keys of the map will be
  // localized in the default browser locale or the closest available fallback.
  //
  // |cb| will be called on the invoking thread, and will be sequenced.
  virtual void ResolveManufacturers(const ResolveManufacturersCallback& cb) = 0;

  // Get all models from a given manufacturer, localized in the
  // default browser locale or the closest available fallback.
  // |manufacturer| must be a value returned from a successful
  // ResolveManufacturers() call performed from this PpdProvider
  // instance.
  //
  // |cb| will be called on the invoking thread, and will be sequenced.
  virtual void ResolvePrinters(const std::string& manufacturer,
                               const ResolvePrintersCallback& cb) = 0;

  // Attempt to find a PpdReference for the given printer.  You should supply
  // as much information in search_data as you can.
  virtual void ResolvePpdReference(const PrinterSearchData& search_data,
                                   const ResolvePpdReferenceCallback& cb) = 0;

  // Given a PpdReference, attempt to get the PPD for printing.
  //
  // |cb| will be called on the invoking thread, and will be sequenced.
  virtual void ResolvePpd(const Printer::PpdReference& reference,
                          const ResolvePpdCallback& cb) = 0;

  // For a given PpdReference, retrieve the make and model strings used to
  // construct that reference.
  virtual void ReverseLookup(const std::string& effective_make_and_model,
                             const ReverseLookupCallback& cb) = 0;

 protected:
  friend class base::RefCounted<PpdProvider>;
  virtual ~PpdProvider() {}
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PPD_PROVIDER_H_
