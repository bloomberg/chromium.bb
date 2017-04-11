// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_provider.h"

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/json/json_parser.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/printing_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace chromeos {
namespace printing {
namespace {

// Returns false if there are obvious errors in the reference that will prevent
// resolution.
bool PpdReferenceIsWellFormed(const Printer::PpdReference& reference) {
  int filled_fields = 0;
  if (!reference.user_supplied_ppd_url.empty()) {
    ++filled_fields;
    GURL tmp_url(reference.user_supplied_ppd_url);
    if (!tmp_url.is_valid() || !tmp_url.SchemeIs("file")) {
      LOG(ERROR) << "Invalid url for a user-supplied ppd: "
                 << reference.user_supplied_ppd_url
                 << " (must be a file:// URL)";
      return false;
    }
  }
  if (!reference.effective_make_and_model.empty()) {
    ++filled_fields;
  }
  // Should have exactly one non-empty field.
  return filled_fields == 1;
}

std::string PpdReferenceToCacheKey(const Printer::PpdReference& reference) {
  DCHECK(PpdReferenceIsWellFormed(reference));
  // The key prefixes here are arbitrary, but ensure we can't have an (unhashed)
  // collision between keys generated from different PpdReference fields.
  if (!reference.effective_make_and_model.empty()) {
    return std::string("em:") + reference.effective_make_and_model;
  } else {
    return std::string("up:") + reference.user_supplied_ppd_url;
  }
}

struct ManufacturerMetadata {
  // Key used to look up the printer list on the server.  This is initially
  // populated.
  std::string reference;

  // Map from localized printer name to canonical-make-and-model string for
  // the given printer.  Populated on demand.
  std::unique_ptr<std::unordered_map<std::string, std::string>> printers;
};

// Data for an inflight USB metadata resolution.
struct UsbDeviceId {
  int vendor_id;
  int device_id;
};

// A queued request to download printer information for a manufacturer.
struct PrinterResolutionQueueEntry {
  // Localized manufacturer name
  std::string manufacturer;

  // URL we are going to pull from.
  GURL url;

  // User callback on completion.
  PpdProvider::ResolvePrintersCallback cb;
};

class PpdProviderImpl : public PpdProvider, public net::URLFetcherDelegate {
 public:
  // What kind of thing is the fetcher currently fetching?  We use this to
  // determine what to do when the fetcher returns a result.
  enum FetcherTarget {
    FT_LOCALES,        // Locales metadata.
    FT_MANUFACTURERS,  // List of manufacturers metadata.
    FT_PRINTERS,       // List of printers from a manufacturer.
    FT_PPD_INDEX,      // Master ppd index.
    FT_PPD,            // A Ppd file.
    FT_USB_DEVICES     // USB device id to canonical name map.
  };

  PpdProviderImpl(
      const std::string& browser_locale,
      scoped_refptr<net::URLRequestContextGetter> url_context_getter,
      scoped_refptr<PpdCache> ppd_cache,
      scoped_refptr<base::SequencedTaskRunner> disk_task_runner,
      const PpdProvider::Options& options)
      : browser_locale_(browser_locale),
        url_context_getter_(url_context_getter),
        ppd_cache_(ppd_cache),
        disk_task_runner_(disk_task_runner),
        options_(options),
        weak_factory_(this) {}

  // Resolving manufacturers requires a couple of steps, because of
  // localization.  First we have to figure out what locale to use, which
  // involves grabbing a list of available locales from the server.  Once we
  // have decided on a locale, we go out and fetch the manufacturers map in that
  // localization.
  //
  // This means when a request comes in, we either queue it and start background
  // fetches if necessary, or we satisfy it immediately from memory.
  void ResolveManufacturers(const ResolveManufacturersCallback& cb) override {
    CHECK(base::SequencedTaskRunnerHandle::IsSet())
        << "ResolveManufacturers must be called from a SequencedTaskRunner"
           "context";
    if (cached_metadata_.get() != nullptr) {
      // We already have this in memory.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(cb, PpdProvider::SUCCESS, GetManufacturerList()));
      return;
    }
    manufacturers_resolution_queue_.push_back(cb);
    MaybeStartFetch();
  }

  // If there is work outstanding that requires a URL fetch to complete, start
  // going on it.
  void MaybeStartFetch() {
    if (fetch_inflight_) {
      // We'll call this again when the outstanding fetch completes.
      return;
    }

    if (!usb_resolution_queue_.empty()) {
      StartFetch(GetUsbURL(usb_resolution_queue_.front().first.vendor_id),
                 FT_USB_DEVICES);
      return;
    }

    if (!manufacturers_resolution_queue_.empty()) {
      if (locale_.empty()) {
        // Don't have a locale yet, figure that out first.
        StartFetch(GetLocalesURL(), FT_LOCALES);
      } else {
        // Get manufacturers based on the locale we have.
        StartFetch(GetManufacturersURL(locale_), FT_MANUFACTURERS);
      }
      return;
    }
    if (!printers_resolution_queue_.empty()) {
      StartFetch(printers_resolution_queue_.front().url, FT_PRINTERS);
      return;
    }
    while (!ppd_resolution_queue_.empty()) {
      const auto& next = ppd_resolution_queue_.front();
      if (!next.first.user_supplied_ppd_url.empty()) {
        DCHECK(next.first.effective_make_and_model.empty());
        GURL url(next.first.user_supplied_ppd_url);
        DCHECK(url.is_valid());
        StartFetch(url, FT_PPD);
        return;
      }
      DCHECK(!next.first.effective_make_and_model.empty());
      if (cached_ppd_index_.get() == nullptr) {
        // Have to have the ppd index before we can resolve by ppd server
        // key.
        StartFetch(GetPpdIndexURL(), FT_PPD_INDEX);
        return;
      }
      // Get the URL from the ppd index and start the fetch.
      auto it = cached_ppd_index_->find(next.first.effective_make_and_model);
      if (it != cached_ppd_index_->end()) {
        StartFetch(GetPpdURL(it->second), FT_PPD);
        return;
      }
      // This ppd reference isn't in the index.  That's not good. Fail
      // out the current resolution and go try to start the next
      // thing if there is one.
      LOG(ERROR) << "PPD " << next.first.effective_make_and_model
                 << " not found in server index";
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(next.second, PpdProvider::INTERNAL_ERROR, std::string()));
      ppd_resolution_queue_.pop_front();
    }
  }

  void ResolvePrinters(const std::string& manufacturer,
                       const ResolvePrintersCallback& cb) override {
    std::unordered_map<std::string, ManufacturerMetadata>::iterator it;
    if (cached_metadata_.get() == nullptr ||
        (it = cached_metadata_->find(manufacturer)) ==
            cached_metadata_->end()) {
      // User error.
      LOG(ERROR) << "Can't resolve printers for unknown manufacturer "
                 << manufacturer;
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(cb, PpdProvider::INTERNAL_ERROR,
                                std::vector<std::string>()));
      return;
    }
    if (it->second.printers.get() != nullptr) {
      // Satisfy from the cache.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(cb, PpdProvider::SUCCESS,
                                GetManufacturerPrinterList(it->second)));
    } else {
      // We haven't resolved this manufacturer yet.
      PrinterResolutionQueueEntry entry;
      entry.manufacturer = manufacturer;
      entry.url = GetPrintersURL(it->second.reference);
      entry.cb = cb;
      printers_resolution_queue_.push_back(entry);
      MaybeStartFetch();
    }
  }

  void ResolveUsbIds(int vendor_id,
                     int device_id,
                     const ResolveUsbIdsCallback& cb) override {
    usb_resolution_queue_.push_back({{vendor_id, device_id}, cb});
    MaybeStartFetch();
  }

  bool GetPpdReference(const std::string& manufacturer,
                       const std::string& printer,
                       Printer::PpdReference* reference) const override {
    std::unordered_map<std::string, ManufacturerMetadata>::iterator top_it;
    if (cached_metadata_.get() == nullptr) {
      return false;
    }
    auto it = cached_metadata_->find(manufacturer);
    if (it == cached_metadata_->end() || it->second.printers.get() == nullptr) {
      return false;
    }
    const auto& printers_map = *it->second.printers;
    auto it2 = printers_map.find(printer);
    if (it2 == printers_map.end()) {
      return false;
    }
    *reference = Printer::PpdReference();
    reference->effective_make_and_model = it2->second;
    return true;
  }

  void ResolvePpd(const Printer::PpdReference& reference,
                  const ResolvePpdCallback& cb) override {
    // Do a sanity check here, so we can assumed |reference| is well-formed in
    // the rest of this class.
    if (!PpdReferenceIsWellFormed(reference)) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(cb, PpdProvider::INTERNAL_ERROR, ""));
      return;
    }
    // First step, check the cache.  If the cache lookup fails, we'll (try to)
    // consult the server.
    ppd_cache_->Find(PpdReferenceToCacheKey(reference),
                     base::Bind(&PpdProviderImpl::ResolvePpdCacheLookupDone,
                                weak_factory_.GetWeakPtr(), reference, cb));
  }

  // Our only sources of long running ops are cache fetches and network fetches.
  bool Idle() const override { return ppd_cache_->Idle() && !fetch_inflight_; }

  // Common handler that gets called whenever a fetch completes.  Note this
  // is used both for |fetcher_| fetches (i.e. http[s]) and file-based fetches;
  // |source| may be null in the latter case.
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    switch (fetcher_target_) {
      case FT_LOCALES:
        OnLocalesFetchComplete();
        break;
      case FT_MANUFACTURERS:
        OnManufacturersFetchComplete();
        break;
      case FT_PRINTERS:
        OnPrintersFetchComplete();
        break;
      case FT_PPD_INDEX:
        OnPpdIndexFetchComplete();
        break;
      case FT_PPD:
        OnPpdFetchComplete();
        break;
      case FT_USB_DEVICES:
        OnUsbFetchComplete();
        break;
      default:
        LOG(DFATAL) << "Unknown fetch source";
    }
    fetch_inflight_ = false;
    MaybeStartFetch();
  }

 private:
  // Return the URL used to look up the supported locales list.
  GURL GetLocalesURL() {
    return GURL(options_.ppd_server_root + "/metadata/locales.json");
  }

  GURL GetUsbURL(int vendor_id) {
    DCHECK_GT(vendor_id, 0);
    DCHECK_LE(vendor_id, 0xffff);

    return GURL(base::StringPrintf("%s/metadata/usb-%04x.json",
                                   options_.ppd_server_root.c_str(),
                                   vendor_id));
  }

  // Return the URL used to get the index of ppd server key -> ppd.
  GURL GetPpdIndexURL() {
    return GURL(options_.ppd_server_root + "/metadata/index.json");
  }

  // Return the URL to get a localized manufacturers map.
  GURL GetManufacturersURL(const std::string& locale) {
    return GURL(base::StringPrintf("%s/metadata/manufacturers-%s.json",
                                   options_.ppd_server_root.c_str(),
                                   locale.c_str()));
  }

  // Return the URL used to get a list of printers from the manufacturer |ref|.
  GURL GetPrintersURL(const std::string& ref) {
    return GURL(base::StringPrintf(
        "%s/metadata/%s", options_.ppd_server_root.c_str(), ref.c_str()));
  }

  // Return the URL used to get a ppd with the given filename.
  GURL GetPpdURL(const std::string& filename) {
    return GURL(base::StringPrintf(
        "%s/ppds/%s", options_.ppd_server_root.c_str(), filename.c_str()));
  }

  // Create and return a fetcher that has the usual (for this class) flags set
  // and calls back to OnURLFetchComplete in this class when it finishes.
  void StartFetch(const GURL& url, FetcherTarget target) {
    DCHECK(!fetch_inflight_);
    DCHECK_EQ(fetcher_.get(), nullptr);
    fetcher_target_ = target;
    fetch_inflight_ = true;

    if (url.SchemeIs("http") || url.SchemeIs("https")) {
      fetcher_ = net::URLFetcher::Create(url, net::URLFetcher::GET, this);
      fetcher_->SetRequestContext(url_context_getter_.get());
      fetcher_->SetLoadFlags(net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
                             net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SEND_AUTH_DATA);
      fetcher_->Start();
    } else if (url.SchemeIs("file")) {
      disk_task_runner_->PostTaskAndReply(
          FROM_HERE, base::Bind(&PpdProviderImpl::FetchFile, this, url),
          base::Bind(&PpdProviderImpl::OnURLFetchComplete, this, nullptr));
    }
  }

  // Fetch the file pointed at by url and store it in |file_fetch_contents_|.
  // Use |file_fetch_success_| to indicate success or failure.
  void FetchFile(const GURL& url) {
    CHECK(url.is_valid());
    CHECK(url.SchemeIs("file"));
    base::ThreadRestrictions::AssertIOAllowed();
    file_fetch_success_ = base::ReadFileToString(base::FilePath(url.path()),
                                                 &file_fetch_contents_);
  }

  // Callback when the cache lookup for a ppd request finishes.  If we hit in
  // the cache, satisfy the resolution, otherwise kick it over to the fetcher
  // queue to be grabbed from a server.
  void ResolvePpdCacheLookupDone(const Printer::PpdReference& reference,
                                 const ResolvePpdCallback& cb,
                                 const PpdCache::FindResult& result) {
    if (result.success) {
      // Cache hit.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(cb, PpdProvider::SUCCESS, result.contents));
    } else {
      // Cache miss.  Queue it to be satisfied by the fetcher queue.
      ppd_resolution_queue_.push_back({reference, cb});
      MaybeStartFetch();
    }
  }

  // Handler for the completion of the locales fetch.  This response should be a
  // list of strings, each of which is a locale in which we can answer queries
  // on the server.  The server (should) guarantee that we get 'en' as an
  // absolute minimum.
  //
  // Combine this information with the browser locale to figure out the best
  // locale to use, and then start a fetch of the manufacturers map in that
  // locale.
  void OnLocalesFetchComplete() {
    std::string contents;
    if (ValidateAndGetResponseAsString(&contents) != PpdProvider::SUCCESS) {
      FailQueuedMetadataResolutions(PpdProvider::SERVER_ERROR);
      return;
    }
    auto top_list = base::ListValue::From(base::JSONReader::Read(contents));

    if (top_list.get() == nullptr) {
      // We got something malformed back.
      FailQueuedMetadataResolutions(PpdProvider::INTERNAL_ERROR);
      return;
    }

    // This should just be a simple list of locale strings.
    std::vector<std::string> available_locales;
    bool found_en = false;
    for (const std::unique_ptr<base::Value>& entry : *top_list) {
      std::string tmp;
      // Locales should have at *least* a two-character country code.  100 is an
      // arbitrary upper bound for length to protect against extreme bogosity.
      if (!entry->GetAsString(&tmp) || tmp.size() < 2 || tmp.size() > 100) {
        FailQueuedMetadataResolutions(PpdProvider::INTERNAL_ERROR);
        return;
      }
      if (tmp == "en") {
        found_en = true;
      }
      available_locales.push_back(tmp);
    }
    if (available_locales.empty() || !found_en) {
      // We have no locales, or we didn't get an english locale (which is our
      // ultimate fallback)
      FailQueuedMetadataResolutions(PpdProvider::INTERNAL_ERROR);
      return;
    }
    // Everything checks out, set the locale, head back to fetch dispatch
    // to start the manufacturer fetch.
    locale_ = GetBestLocale(available_locales);
  }

  // Called when the |fetcher_| is expected have the results of a
  // manufacturer map (which maps localized manufacturer names to keys for
  // looking up printers from that manufacturer).  Use this information to
  // populate manufacturer_map_, and resolve all queued ResolveManufacturers()
  // calls.
  void OnManufacturersFetchComplete() {
    DCHECK_EQ(nullptr, cached_metadata_.get());
    std::vector<std::pair<std::string, std::string>> contents;
    PpdProvider::CallbackResultCode code =
        ValidateAndParseJSONResponse(&contents);
    if (code != PpdProvider::SUCCESS) {
      LOG(ERROR) << "Failed manufacturer parsing";
      FailQueuedMetadataResolutions(code);
      return;
    }
    cached_metadata_ = base::MakeUnique<
        std::unordered_map<std::string, ManufacturerMetadata>>();

    for (const auto& entry : contents) {
      ManufacturerMetadata value;
      value.reference = entry.second;
      (*cached_metadata_)[entry.first].reference = entry.second;
    }

    std::vector<std::string> manufacturer_list = GetManufacturerList();
    // Complete any queued manufacturer resolutions.
    for (const auto& cb : manufacturers_resolution_queue_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(cb, PpdProvider::SUCCESS, manufacturer_list));
    }
    manufacturers_resolution_queue_.clear();
  }

  // The outstanding fetch associated with the front of
  // |printers_resolution_queue_| finished, use the response to satisfy that
  // ResolvePrinters() call.
  void OnPrintersFetchComplete() {
    CHECK(cached_metadata_.get() != nullptr);
    DCHECK(!printers_resolution_queue_.empty());
    std::vector<std::pair<std::string, std::string>> contents;
    PpdProvider::CallbackResultCode code =
        ValidateAndParseJSONResponse(&contents);
    if (code != PpdProvider::SUCCESS) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(printers_resolution_queue_.front().cb, code,
                                std::vector<std::string>()));
    } else {
      // This should be a list of lists of 2-element strings, where the first
      // element is the localized name of the printer and the second element
      // is the canonical name of the printer.
      const std::string& manufacturer =
          printers_resolution_queue_.front().manufacturer;
      auto it = cached_metadata_->find(manufacturer);

      // If we kicked off a resolution, the entry better have already been
      // in the map.
      CHECK(it != cached_metadata_->end());

      // Create the printer map in the cache, and populate it.
      auto& manufacturer_metadata = it->second;
      CHECK(manufacturer_metadata.printers.get() == nullptr);
      manufacturer_metadata.printers =
          base::MakeUnique<std::unordered_map<std::string, std::string>>();

      for (const auto& entry : contents) {
        manufacturer_metadata.printers->insert({entry.first, entry.second});
      }
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(printers_resolution_queue_.front().cb,
                     PpdProvider::SUCCESS,
                     GetManufacturerPrinterList(manufacturer_metadata)));
    }
    printers_resolution_queue_.pop_front();
  }

  // Called when |fetcher_| should have just received the index mapping
  // ppd server keys to ppd filenames.  Use this to populate
  // |cached_ppd_index_|.
  void OnPpdIndexFetchComplete() {
    std::vector<std::pair<std::string, std::string>> contents;
    PpdProvider::CallbackResultCode code =
        ValidateAndParseJSONResponse(&contents);
    if (code != PpdProvider::SUCCESS) {
      FailQueuedServerPpdResolutions(code);
    } else {
      cached_ppd_index_ =
          base::MakeUnique<std::unordered_map<std::string, std::string>>();
      // This should be a list of lists of 2-element strings, where the first
      // element is the |effective_make_and_model| of the printer and the second
      // element is the filename of the ppd in the ppds/ directory on the
      // server.
      for (const auto& entry : contents) {
        cached_ppd_index_->insert(entry);
      }
    }
  }

  // This is called when |fetcher_| should have just downloaded a ppd.  If we
  // downloaded something successfully, use it to satisfy the front of the ppd
  // resolution queue, otherwise fail out that resolution.
  void OnPpdFetchComplete() {
    DCHECK(!ppd_resolution_queue_.empty());
    std::string contents;
    if ((ValidateAndGetResponseAsString(&contents) != PpdProvider::SUCCESS) ||
        contents.size() > kMaxPpdSizeBytes) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(ppd_resolution_queue_.front().second,
                                PpdProvider::SERVER_ERROR, std::string()));
    } else {
      ppd_cache_->Store(
          PpdReferenceToCacheKey(ppd_resolution_queue_.front().first), contents,

          base::Callback<void()>());
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(ppd_resolution_queue_.front().second,
                                PpdProvider::SUCCESS, contents));
    }
    ppd_resolution_queue_.pop_front();
  }

  // Called when |fetcher_| should have just downloaded a usb device map
  // for the vendor at the head of the |usb_resolution_queue_|.
  void OnUsbFetchComplete() {
    DCHECK(!usb_resolution_queue_.empty());
    std::string contents;
    std::string buffer;
    PpdProvider::CallbackResultCode result =
        ValidateAndGetResponseAsString(&buffer);
    int desired_device_id = usb_resolution_queue_.front().first.device_id;
    if (result == PpdProvider::SUCCESS) {
      // Parse the JSON response.  This should be a list of the form
      // [
      //  [0x3141, "some canonical name"],
      //  [0x5926, "some othercanonical name"]
      // ]
      // So we scan through the response looking for our desired device id.
      auto top_list = base::ListValue::From(base::JSONReader::Read(buffer));

      if (top_list.get() == nullptr) {
        // We got something malformed back.
        LOG(ERROR) << "Malformed top list";
        result = PpdProvider::INTERNAL_ERROR;
      } else {
        // We'll set result to SUCCESS if we do find the device.
        result = PpdProvider::NOT_FOUND;
        for (const auto& entry : *top_list) {
          int device_id;
          base::ListValue* sub_list;

          // Each entry should be a size-2 list with an integer and a string.
          if (!entry->GetAsList(&sub_list) || sub_list->GetSize() != 2 ||
              !sub_list->GetInteger(0, &device_id) ||
              !sub_list->GetString(1, &contents) || device_id < 0 ||
              device_id > 0xffff) {
            // Malformed data.
            LOG(ERROR) << "Malformed line in usb device list";
            result = PpdProvider::INTERNAL_ERROR;
            break;
          }
          if (device_id == desired_device_id) {
            // Found it.
            result = PpdProvider::SUCCESS;
            break;
          }
        }
      }
    }
    if (result != PpdProvider::SUCCESS) {
      contents.clear();
    }
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(usb_resolution_queue_.front().second, result, contents));
    usb_resolution_queue_.pop_front();
  }

  // Something went wrong during metadata fetches.  Fail all queued metadata
  // resolutions with the given error code.
  void FailQueuedMetadataResolutions(PpdProvider::CallbackResultCode code) {
    for (const auto& cb : manufacturers_resolution_queue_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(cb, code, std::vector<std::string>()));
    }
    manufacturers_resolution_queue_.clear();
  }

  // Fail all server-based ppd resolutions inflight, because we failed to grab
  // the necessary index data from the server.  Note we leave any user-based ppd
  // resolutions intact, as they don't depend on the data we're missing.
  void FailQueuedServerPpdResolutions(PpdProvider::CallbackResultCode code) {
    std::deque<std::pair<Printer::PpdReference, ResolvePpdCallback>>
        filtered_queue;

    for (const auto& entry : ppd_resolution_queue_) {
      if (!entry.first.user_supplied_ppd_url.empty()) {
        filtered_queue.push_back(entry);
      } else {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::Bind(entry.second, code, std::string()));
      }
    }
    ppd_resolution_queue_ = std::move(filtered_queue);
  }

  // Given a list of possible locale strings (e.g. 'en-GB'), determine which of
  // them we should use to best serve results for the browser locale (which was
  // given to us at construction time).
  std::string GetBestLocale(const std::vector<std::string>& available_locales) {
    // First look for an exact match.  If we find one, just use that.
    for (const std::string& available : available_locales) {
      if (available == browser_locale_) {
        return available;
      }
    }

    // Next, look for an available locale that is a parent of browser_locale_.
    // Return the most specific one.  For example, if we want 'en-GB-foo' and we
    // don't have an exact match, but we do have 'en-GB' and 'en', we will
    // return 'en-GB' -- the most specific match which is a parent of the
    // requested locale.
    size_t best_len = 0;
    size_t best_idx = -1;
    for (size_t i = 0; i < available_locales.size(); ++i) {
      const std::string& available = available_locales[i];
      if (base::StringPiece(browser_locale_).starts_with(available + "-") &&
          available.size() > best_len) {
        best_len = available.size();
        best_idx = i;
      }
    }
    if (best_idx != static_cast<size_t>(-1)) {
      return available_locales[best_idx];
    }

    // Last chance for a match, look for the locale that matches the *most*
    // pieces of locale_, with ties broken by being least specific.  So for
    // example, if we have 'es-GB', 'es-GB-foo' but no 'es' available, and we're
    // requesting something for 'es', we'll get back 'es-GB' -- the least
    // specific thing that matches some of the locale.
    std::vector<base::StringPiece> browser_locale_pieces =
        base::SplitStringPiece(browser_locale_, "-", base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_ALL);
    size_t best_match_size = 0;
    size_t best_match_specificity;
    best_idx = -1;
    for (size_t i = 0; i < available_locales.size(); ++i) {
      const std::string& available = available_locales[i];
      std::vector<base::StringPiece> available_pieces = base::SplitStringPiece(
          available, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      size_t match_size = 0;
      for (; match_size < available_pieces.size() &&
             match_size < browser_locale_pieces.size();
           ++match_size) {
        if (available_pieces[match_size] != browser_locale_pieces[match_size]) {
          break;
        }
      }
      if (match_size > 0 &&
          (best_idx == static_cast<size_t>(-1) ||
           match_size > best_match_size ||
           (match_size == best_match_size &&
            available_pieces.size() < best_match_specificity))) {
        best_idx = i;
        best_match_size = match_size;
        best_match_specificity = available_pieces.size();
      }
    }
    if (best_idx != static_cast<size_t>(-1)) {
      return available_locales[best_idx];
    }

    // Everything else failed.  Throw up our hands and default to english.
    return "en";
  }

  // Get the results of a fetch.  This is a little tricky, because a fetch
  // may have been done by |fetcher_|, or it may have been a file access, in
  // which case we want to look at |file_fetch_contents_|.  We distinguish
  // between the cases based on whether or not |fetcher_| is null.
  //
  // We return NOT_FOUND for 404 or file not found, SERVER_ERROR for other
  // errors, SUCCESS if everything was good.
  CallbackResultCode ValidateAndGetResponseAsString(std::string* contents) {
    CallbackResultCode ret;
    if (fetcher_.get() != nullptr) {
      if (fetcher_->GetStatus().status() != net::URLRequestStatus::SUCCESS) {
        ret = PpdProvider::SERVER_ERROR;
      } else if (fetcher_->GetResponseCode() != net::HTTP_OK) {
        if (fetcher_->GetResponseCode() == net::HTTP_NOT_FOUND) {
          // A 404 means not found, everything else is a server error.
          ret = PpdProvider::NOT_FOUND;
        } else {
          ret = PpdProvider::SERVER_ERROR;
        }
      } else {
        fetcher_->GetResponseAsString(contents);
        ret = PpdProvider::SUCCESS;
      }
      fetcher_.reset();
    } else {
      // It's a file load.
      if (file_fetch_success_) {
        *contents = file_fetch_contents_;
      } else {
        contents->clear();
      }
      // A failure to load a file is always considered a NOT FOUND error (even
      // if the underlying causes is lack of access or similar, this seems to be
      // the best match for intent.
      ret = file_fetch_success_ ? PpdProvider::SUCCESS : PpdProvider::NOT_FOUND;
      file_fetch_contents_.clear();
    }
    return ret;
  }

  // Many of our metadata fetches happens to be in the form of a JSON
  // list-of-lists-of-2-strings.  So this just attempts to parse a JSON reply to
  // |fetcher| into the passed contents vector.  A return code of SUCCESS means
  // the JSON was formatted as expected and we've parsed it into |contents|.  On
  // error the contents of |contents| cleared.
  PpdProvider::CallbackResultCode ValidateAndParseJSONResponse(
      std::vector<std::pair<std::string, std::string>>* contents) {
    contents->clear();
    std::string buffer;
    auto tmp = ValidateAndGetResponseAsString(&buffer);
    if (tmp != PpdProvider::SUCCESS) {
      return tmp;
    }
    auto top_list = base::ListValue::From(base::JSONReader::Read(buffer));

    if (top_list.get() == nullptr) {
      // We got something malformed back.
      return PpdProvider::INTERNAL_ERROR;
    }
    for (const auto& entry : *top_list) {
      base::ListValue* sub_list;
      contents->push_back({});
      if (!entry->GetAsList(&sub_list) || sub_list->GetSize() != 2 ||
          !sub_list->GetString(0, &contents->back().first) ||
          !sub_list->GetString(1, &contents->back().second)) {
        contents->clear();
        return PpdProvider::INTERNAL_ERROR;
      }
    }
    return PpdProvider::SUCCESS;
  }

  // Create the list of manufacturers from |cached_metadata_|.  Requires that
  // the manufacturer list has already been resolved.
  std::vector<std::string> GetManufacturerList() const {
    CHECK(cached_metadata_.get() != nullptr);
    std::vector<std::string> ret;
    ret.reserve(cached_metadata_->size());
    for (const auto& entry : *cached_metadata_) {
      ret.push_back(entry.first);
    }
    // TODO(justincarlson) -- this should be a localization-aware sort.
    sort(ret.begin(), ret.end());
    return ret;
  }

  // Get the list of printers from a given manufacturer from |cached_metadata_|.
  // Requires that we have already resolved this from the server.
  std::vector<std::string> GetManufacturerPrinterList(
      const ManufacturerMetadata& meta) const {
    CHECK(meta.printers.get() != nullptr);
    std::vector<std::string> ret;
    ret.reserve(meta.printers->size());
    for (const auto& entry : *meta.printers) {
      ret.push_back(entry.first);
    }
    // TODO(justincarlson) -- this should be a localization-aware sort.
    sort(ret.begin(), ret.end());
    return ret;
  }

  // Map from (localized) manufacturer name to metadata for that manufacturer.
  // This is populated lazily.  If we don't yet have a manufacturer list, the
  // top pointer will be null.  When we create the top level map, then each
  // value will only contain a reference which can be used to resolve the
  // printer list from that manufacturer.  On demand, we use these references to
  // resolve the actual printer lists.
  std::unique_ptr<std::unordered_map<std::string, ManufacturerMetadata>>
      cached_metadata_;

  // Cached contents of the server index, which maps a
  // PpdReference::effective_make_and_model to a url for the corresponding
  // ppd.
  // Null until we have fetched the index.
  std::unique_ptr<std::unordered_map<std::string, std::string>>
      cached_ppd_index_;

  // Queued ResolveManufacturers() calls.  We will simultaneously resolve
  // all queued requests, so no need for a deque here.
  std::vector<ResolveManufacturersCallback> manufacturers_resolution_queue_;

  // Queued ResolvePrinters() calls.
  std::deque<PrinterResolutionQueueEntry> printers_resolution_queue_;

  // Queued ResolvePpd() requests.
  std::deque<std::pair<Printer::PpdReference, ResolvePpdCallback>>
      ppd_resolution_queue_;

  // Queued ResolveUsbIds() requests.
  std::deque<std::pair<UsbDeviceId, ResolveUsbIdsCallback>>
      usb_resolution_queue_;

  // Locale we're using for grabbing stuff from the server.  Empty if we haven't
  // determined it yet.
  std::string locale_;

  // If the fetcher is active, what's it fetching?
  FetcherTarget fetcher_target_;

  // Fetcher used for all network fetches.  This is explicitly reset() when
  // a fetch has been processed.
  std::unique_ptr<net::URLFetcher> fetcher_;
  bool fetch_inflight_ = false;

  // Locale of the browser, as returned by
  // BrowserContext::GetApplicationLocale();
  const std::string browser_locale_;

  scoped_refptr<net::URLRequestContextGetter> url_context_getter_;

  // For file:// fetches, a staging buffer and result flag for loading the file.
  std::string file_fetch_contents_;
  bool file_fetch_success_;

  // Cache of ppd files.
  scoped_refptr<PpdCache> ppd_cache_;

  // Where to run disk operations.
  scoped_refptr<base::SequencedTaskRunner> disk_task_runner_;

  // Construction-time options, immutable.
  const PpdProvider::Options options_;

  base::WeakPtrFactory<PpdProviderImpl> weak_factory_;

 protected:
  ~PpdProviderImpl() override {}
};

}  // namespace

// static
scoped_refptr<PpdProvider> PpdProvider::Create(
    const std::string& browser_locale,
    scoped_refptr<net::URLRequestContextGetter> url_context_getter,
    scoped_refptr<PpdCache> ppd_cache,
    scoped_refptr<base::SequencedTaskRunner> disk_task_runner,
    const PpdProvider::Options& options) {
  return scoped_refptr<PpdProvider>(
      new PpdProviderImpl(browser_locale, url_context_getter, ppd_cache,
                          disk_task_runner, options));
}
}  // namespace printing
}  // namespace chromeos
