// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_cache.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"
#include "net/base/io_buffer.h"
#include "net/filter/filter.h"
#include "net/filter/gzip_header.h"

using base::FilePath;
using std::string;

namespace chromeos {
namespace printing {
namespace {

// Return true if it looks like contents is already gzipped, false otherwise.
bool IsGZipped(const string& contents) {
  const char* ignored;
  net::GZipHeader header;
  return header.ReadMore(contents.data(), contents.size(), &ignored) ==
         net::GZipHeader::COMPLETE_HEADER;
}

class PpdCacheImpl : public PpdCache {
 public:
  explicit PpdCacheImpl(const FilePath& cache_base_dir)
      : cache_base_dir_(cache_base_dir) {}
  ~PpdCacheImpl() override {}

  // Public API functions.
  base::Optional<FilePath> Find(
      const Printer::PpdReference& reference) const override {
    base::Optional<FilePath> ret;

    // We can't know here if we have a gzipped or un-gzipped version, so just
    // look for both.
    FilePath contents_path_base = GetCachePathBase(reference);
    for (const string& extension : {".ppd", ".ppd.gz"}) {
      FilePath contents_path = contents_path_base.AddExtension(extension);
      if (base::PathExists(contents_path)) {
        ret = contents_path;
        break;
      }
    }
    return ret;
  }

  base::Optional<FilePath> Store(const Printer::PpdReference& reference,
                                 const string& ppd_contents) override {
    base::Optional<FilePath> ret;
    FilePath contents_path = GetCachePathBase(reference).AddExtension(".ppd");
    if (IsGZipped(ppd_contents)) {
      contents_path = contents_path.AddExtension(".gz");
    }
    if (base::WriteFile(contents_path, ppd_contents.data(),
                        ppd_contents.size()) ==
        static_cast<int>(ppd_contents.size())) {
      ret = contents_path;
    } else {
      LOG(ERROR) << "Failed to write " << contents_path.LossyDisplayName();
      // Try to clean up the file, as it may have partial contents.  Note that
      // DeleteFile(nonexistant file) should return true, so failure here means
      // something is exceptionally hosed.
      if (!base::DeleteFile(contents_path, false)) {
        LOG(ERROR) << "Failed to cleanup partially-written file "
                   << contents_path.LossyDisplayName();
        return ret;
      }
    }
    return ret;
  }

 private:
  // Get the file path at which we expect to find a PPD if it's cached.
  //
  // This is, ultimately, just a hash function.  It's extremely infrequently
  // used (called once when trying to look up information on a printer or store
  // a PPD), and should be stable, as changing the function will make previously
  // cached entries unfindable, causing resolve logic to be reinvoked
  // unnecessarily.
  //
  // There's also a faint possibility that a bad actor might try to do something
  // nefarious by intentionally causing a cache collision that makes the wrong
  // PPD be used for a printer.  There's no obvious attack vector, but
  // there's also no real cost to being paranoid here, so we use SHA-256 as the
  // underlying hash function, and inject fixed field prefixes to prevent
  // field-substitution spoofing.  This also buys us hash function stability at
  // the same time.
  //
  // Also, care should be taken to preserve the existing hash values if new
  // fields are added to PpdReference -- that is, if a new field F is added
  // to PpdReference,  a PpdReference with a default F value should hash to
  // the same thing as a PpdReference that predates the addition of F to the
  // structure.
  //
  // Note this function expects that the caller will append ".ppd", or ".ppd.gz"
  // to the output as needed.
  FilePath GetCachePathBase(const Printer::PpdReference& ref) const {
    std::vector<string> pieces;
    if (!ref.user_supplied_ppd_url.empty()) {
      pieces.push_back("user_supplied_ppd_url:");
      pieces.push_back(ref.user_supplied_ppd_url);
    } else if (!ref.effective_manufacturer.empty() &&
               !ref.effective_model.empty()) {
      pieces.push_back("manufacturer:");
      pieces.push_back(ref.effective_manufacturer);
      pieces.push_back("model:");
      pieces.push_back(ref.effective_model);
    } else {
      NOTREACHED() << "PpdCache hashing empty PpdReference";
    }
    // The separator here is not needed, but makes debug output more readable.
    string full_key = base::JoinString(pieces, "|");
    string hashed_key = crypto::SHA256HashString(full_key);
    string ascii_hash = base::HexEncode(hashed_key.data(), hashed_key.size());
    VLOG(3) << "PPD Cache key is " << full_key << " which hashes to "
            << ascii_hash;

    return cache_base_dir_.Append(ascii_hash);
  }

  const FilePath cache_base_dir_;

  DISALLOW_COPY_AND_ASSIGN(PpdCacheImpl);
};

}  // namespace

// static
std::unique_ptr<PpdCache> PpdCache::Create(const FilePath& cache_base_dir) {
  return base::MakeUnique<PpdCacheImpl>(cache_base_dir);
}

}  // namespace printing
}  // namespace chromeos
