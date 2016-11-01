// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_cache.h"

#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/json/json_parser.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/base/io_buffer.h"
#include "net/filter/filter.h"
#include "net/filter/gzip_header.h"

namespace chromeos {
namespace printing {
namespace {

// Name of the file we use to cache the list of available printer drivers from
// QuirksServer.  This file resides in the cache directory.
const char kAvailablePrintersFilename[] = "all_printers.json";

// Return true if it looks like contents is already gzipped, false otherwise.
bool IsGZipped(const std::string& contents) {
  const char* ignored;
  net::GZipHeader header;
  return header.ReadMore(contents.data(), contents.size(), &ignored) ==
         net::GZipHeader::COMPLETE_HEADER;
}

class PpdCacheImpl : public PpdCache {
 public:
  PpdCacheImpl(const base::FilePath& cache_base_dir,
               const PpdCache::Options& options)
      : cache_base_dir_(cache_base_dir),
        available_printers_file_(
            cache_base_dir.Append(kAvailablePrintersFilename)),
        options_(options) {}
  ~PpdCacheImpl() override {}

  // Public API functions.
  base::Optional<base::FilePath> Find(
      const Printer::PpdReference& reference) const override {
    base::Optional<base::FilePath> ret;

    // We can't know here if we have a gzipped or un-gzipped version, so just
    // look for both.
    base::FilePath contents_path_base = GetCachePathBase(reference);
    for (const std::string& extension : {".ppd", ".ppd.gz"}) {
      base::FilePath contents_path = contents_path_base.AddExtension(extension);
      if (base::PathExists(contents_path)) {
        ret = contents_path;
        break;
      }
    }
    return ret;
  }

  base::Optional<base::FilePath> Store(
      const Printer::PpdReference& reference,
      const std::string& ppd_contents) override {
    base::Optional<base::FilePath> ret;
    base::FilePath contents_path =
        GetCachePathBase(reference).AddExtension(".ppd");
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

  base::Optional<PpdProvider::AvailablePrintersMap> FindAvailablePrinters()
      override {
    std::string buf;
    if (!MaybeReadAvailablePrintersCache(&buf)) {
      return base::nullopt;
    }
    auto dict = base::DictionaryValue::From(base::JSONReader::Read(buf));
    if (dict == nullptr) {
      LOG(ERROR) << "Failed to deserialize available printers cache";
      return base::nullopt;
    }
    PpdProvider::AvailablePrintersMap ret;
    const base::ListValue* models;
    std::string model;
    for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd();
         it.Advance()) {
      auto& out = ret[it.key()];
      if (!it.value().GetAsList(&models)) {
        LOG(ERROR) << "Skipping malformed printer make: " << it.key();
        continue;
      }
      for (const auto& model_value : *models) {
        if (model_value->GetAsString(&model)) {
          out.push_back(model);
        } else {
          LOG(ERROR) << "Skipping malformed printer model in: " << it.key()
                     << ".  Expected a string, found a "
                     << base::Value::GetTypeName(model_value->GetType());
        }
      }
    }
    return ret;
  }

  // Note we throw up our hands and fail (gracefully) to store if we encounter
  // non-unicode things in the strings of |available_printers|.  Since these
  // strings come from a source we control, being less paranoid about these
  // values seems reasonable.
  void StoreAvailablePrinters(
      const PpdProvider::AvailablePrintersMap& available_printers) override {
    // Convert the map to Values, in preparation for jsonification.
    base::DictionaryValue top_level;
    for (const auto& entry : available_printers) {
      auto printers = base::MakeUnique<base::ListValue>();
      printers->AppendStrings(entry.second);
      top_level.Set(entry.first, std::move(printers));
    }
    std::string contents;
    if (!base::JSONWriter::Write(top_level, &contents)) {
      LOG(ERROR) << "Failed to generate JSON";
      return;
    }
    if (contents.size() > options_.max_available_list_cached_size) {
      LOG(ERROR) << "Serialized available printers list too large (size is "
                 << contents.size() << " bytes)";
      return;
    }
    if (base::WriteFile(available_printers_file_, contents.data(),
                        contents.size()) != static_cast<int>(contents.size())) {
      LOG(ERROR) << "Failed to write available printers cache";
    }
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
  base::FilePath GetCachePathBase(const Printer::PpdReference& ref) const {
    std::vector<std::string> pieces;
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
    std::string full_key = base::JoinString(pieces, "|");
    std::string hashed_key = crypto::SHA256HashString(full_key);
    std::string ascii_hash =
        base::HexEncode(hashed_key.data(), hashed_key.size());
    VLOG(3) << "PPD Cache key is " << full_key << " which hashes to "
            << ascii_hash;

    return cache_base_dir_.Append(ascii_hash);
  }

  // Try to read the available printers cache.  Returns true on success.  On
  // success, |buf| will contain the contents of the file, otherwise it will be
  // cleared.
  bool MaybeReadAvailablePrintersCache(std::string* buf) {
    buf->clear();

    base::File cache_file(available_printers_file_,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File::Info info;
    if (cache_file.IsValid() && cache_file.GetInfo(&info) &&
        (base::Time::Now() - info.last_modified <=
         options_.max_available_list_staleness)) {
      // We have a file that's recent enough to use.
      if (!base::ReadFileToStringWithMaxSize(
              available_printers_file_, buf,
              options_.max_available_list_cached_size)) {
        LOG(ERROR) << "Failed to read printer cache";
        buf->clear();
        return false;
      }
      return true;
    }
    // Either we don't have an openable file, or it's too old.
    //
    // If we have an invalid file and it's not valid for reasons other than
    // NOT_FOUND, that's unexpected and worth logging.  Otherwise this is
    // a normal cache miss.
    if (!cache_file.IsValid() &&
        cache_file.error_details() != base::File::FILE_ERROR_NOT_FOUND) {
      LOG(ERROR) << "Unexpected result when attempting to open printer cache: "
                 << base::File::ErrorToString(cache_file.error_details());
    }
    return false;
  }

  const base::FilePath cache_base_dir_;
  const base::FilePath available_printers_file_;
  const PpdCache::Options options_;

  DISALLOW_COPY_AND_ASSIGN(PpdCacheImpl);
};

}  // namespace

// static
std::unique_ptr<PpdCache> PpdCache::Create(const base::FilePath& cache_base_dir,
                                           const PpdCache::Options& options) {
  return base::MakeUnique<PpdCacheImpl>(cache_base_dir, options);
}

}  // namespace printing
}  // namespace chromeos
