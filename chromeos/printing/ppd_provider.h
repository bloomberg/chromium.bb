// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PPD_PROVIDER_H_
#define CHROMEOS_PRINTING_PPD_PROVIDER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
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
class CHROMEOS_EXPORT PpdProvider {
 public:
  // Possible results of a Resolve* call.
  enum ResolveResult {
    // Found a PPD
    SUCCESS,

    // TODO(justincarlson) - Should we have a "FALLBACK" result here indicating
    // we found a PPD that we think will work, but wasn't an exact match?

    // Looked for a PPD for this configuration, but couldn't find a match.
    NOT_FOUND,

    // Failed to contact an external server needed to finish resolution.
    SERVER_ERROR,

    // Other error that is not expected to be transient.
    INTERNAL_ERROR,
  };

  // Result of a resolve function.  If ResolveResult is SUCCESS, then filepath
  // holds the path to a PPD file (that may or may not be gzipped).  Otherwise,
  // the FilePath will be empty.
  using ResolveCallback = base::Callback<void(ResolveResult, base::FilePath)>;

  // Construction-time options.  Everything in this structure should have
  // a sane default.
  struct Options {
    Options() {}

    // hostname of quirks server to query.
    std::string quirks_server = "chromeosquirksserver-pa.googleapis.com";

    // Maximum size of the contents of a PPD file, in bytes.  Trying to use a
    // PPD file bigger than this will cause INTERNAL_ERRORs at resolution time.
    size_t max_ppd_contents_size_ = 100 * 1024;
  };

  // Create and return a new PpdProvider with the given cache and options.
  static std::unique_ptr<PpdProvider> Create(
      const std::string& api_key,
      scoped_refptr<net::URLRequestContextGetter> url_context_getter,
      std::unique_ptr<PpdCache> cache,
      const Options& options = Options());

  virtual ~PpdProvider() {}

  // Given a PpdReference, attempt to resolve the PPD for printing.
  //
  // Must be called from a Sequenced Task context (i.e.
  // base::SequencedTaskRunnerHandle::IsSet() must be true).
  //
  // |cb| will only be called after the task invoking Resolve() is finished.
  //
  // Only one Resolve call should be outstanding at a time.
  virtual void Resolve(const Printer::PpdReference& ppd_reference,
                       ResolveCallback cb) = 0;

  // Abort any outstanding Resolve call.  After this returns, it is guaranteed
  // that all no ResolveCallback will be called until the next time Resolve is
  // called.  It is a nop to call this if no Resolve is outstanding.
  virtual void AbortResolve() = 0;

  // Most of the time, the cache is just an invisible backend to the Provider,
  // consulted at Resolve time, but in the case of the user doing "Add Printer"
  // and "Select PPD" locally, then we get into a state where we want to put
  // whatever they give us directly into the cache without doing a resolve.
  // This hook lets is do that.
  virtual bool CachePpd(const Printer::PpdReference& ppd_reference,
                        const base::FilePath& ppd_path) = 0;
};

}  // namespace printing
}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PPD_PROVIDER_H_
