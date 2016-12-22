// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_provider.h"

#include <unordered_map>
#include <utility>

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/json/json_parser.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/printing/ppd_cache.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace chromeos {
namespace printing {
namespace {

// Expected fields from the quirks server.
const char kJSONPPDKey[] = "compressedPpd";
const char kJSONLastUpdatedKey[] = "lastUpdatedTime";
const char kJSONTopListKey[] = "manufacturers";
const char kJSONManufacturer[] = "manufacturer";
const char kJSONModelList[] = "models";

class PpdProviderImpl;

// PpdProvider handles two types of URL fetches, and so uses these delegates
// to route OnURLFetchComplete to the appropriate handler.
class ResolveURLFetcherDelegate : public net::URLFetcherDelegate {
 public:
  explicit ResolveURLFetcherDelegate(PpdProviderImpl* parent)
      : parent_(parent) {}

  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Link back to parent.  Not owned.
  PpdProviderImpl* const parent_;
};

class QueryAvailableURLFetcherDelegate : public net::URLFetcherDelegate {
 public:
  explicit QueryAvailableURLFetcherDelegate(PpdProviderImpl* parent)
      : parent_(parent) {}

  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Link back to parent.  Not owned.
  PpdProviderImpl* const parent_;
};

// Data involved in an active Resolve() URL fetch.
struct ResolveFetchData {
  // The fetcher doing the fetch.
  std::unique_ptr<net::URLFetcher> fetcher;

  // The reference being resolved.
  Printer::PpdReference ppd_reference;

  // Callback to invoke on completion.
  PpdProvider::ResolveCallback done_callback;
};

// Data involved in an active QueryAvailable() URL fetch.
struct QueryAvailableFetchData {
  // The fetcher doing the fetch.
  std::unique_ptr<net::URLFetcher> fetcher;

  // Callback to invoke on completion.
  PpdProvider::QueryAvailableCallback done_callback;
};

class PpdProviderImpl : public PpdProvider {
 public:
  PpdProviderImpl(
      const std::string& api_key,
      scoped_refptr<net::URLRequestContextGetter> url_context_getter,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      std::unique_ptr<PpdCache> cache,
      const PpdProvider::Options& options)
      : api_key_(api_key),
        url_context_getter_(url_context_getter),
        io_task_runner_(io_task_runner),
        cache_(std::move(cache)),
        options_(options),
        resolve_delegate_(this),
        query_available_delegate_(this),
        weak_factory_(this) {
    CHECK_GT(options_.max_ppd_contents_size_, 0U);
  }
  ~PpdProviderImpl() override {}

  void Resolve(const Printer::PpdReference& ppd_reference,
               const PpdProvider::ResolveCallback& cb) override {
    CHECK(!cb.is_null());
    CHECK(base::SequencedTaskRunnerHandle::IsSet())
        << "Resolve must be called from a SequencedTaskRunner context";
    auto cache_result = base::MakeUnique<base::Optional<base::FilePath>>();
    auto* raw_cache_result_ptr = cache_result.get();
    bool post_result = io_task_runner_->PostTaskAndReply(
        FROM_HERE, base::Bind(&PpdProviderImpl::ResolveDoCacheLookup,
                              weak_factory_.GetWeakPtr(), ppd_reference,
                              raw_cache_result_ptr),
        base::Bind(&PpdProviderImpl::ResolveCacheLookupDone,
                   weak_factory_.GetWeakPtr(), ppd_reference, cb,
                   base::Passed(&cache_result)));
    DCHECK(post_result);
  }

  void QueryAvailable(const QueryAvailableCallback& cb) override {
    CHECK(!cb.is_null());
    CHECK(base::SequencedTaskRunnerHandle::IsSet())
        << "QueryAvailable() must be called from a SequencedTaskRunner context";
    auto cache_result =
        base::MakeUnique<base::Optional<PpdProvider::AvailablePrintersMap>>();
    auto* raw_cache_result_ptr = cache_result.get();
    bool post_result = io_task_runner_->PostTaskAndReply(
        FROM_HERE, base::Bind(&PpdProviderImpl::QueryAvailableDoCacheLookup,
                              weak_factory_.GetWeakPtr(), raw_cache_result_ptr),
        base::Bind(&PpdProviderImpl::QueryAvailableCacheLookupDone,
                   weak_factory_.GetWeakPtr(), cb,
                   base::Passed(&cache_result)));
    DCHECK(post_result);
  }

  bool CachePpd(const Printer::PpdReference& ppd_reference,
                const base::FilePath& ppd_path) override {
    std::string buf;
    if (!base::ReadFileToStringWithMaxSize(ppd_path, &buf,
                                           options_.max_ppd_contents_size_)) {
      return false;
    }
    return static_cast<bool>(cache_->Store(ppd_reference, buf));
  }

  // Called on the same thread as Resolve() when the fetcher completes its
  // fetch.
  void OnResolveFetchComplete(const net::URLFetcher* source) {
    std::unique_ptr<ResolveFetchData> fetch_data = GetResolveFetchData(source);
    std::string ppd_contents;
    uint64_t last_updated_time;
    if (!ExtractResolveResponseData(source, fetch_data.get(), &ppd_contents,
                                    &last_updated_time)) {
      fetch_data->done_callback.Run(PpdProvider::SERVER_ERROR,
                                    base::FilePath());
      return;
    }

    auto ppd_file = cache_->Store(fetch_data->ppd_reference, ppd_contents);
    if (!ppd_file) {
      // Failed to store.
      fetch_data->done_callback.Run(PpdProvider::INTERNAL_ERROR,
                                    base::FilePath());
      return;
    }
    fetch_data->done_callback.Run(PpdProvider::SUCCESS, ppd_file.value());
  }

  // Called on the same thread as QueryAvailable() when the cache lookup is
  // done.  If the cache satisfied the request, finish the Query, otherwise
  // start a URL request to satisfy the Query.  This runs on the same thread as
  // QueryAvailable() was invoked on.
  void QueryAvailableCacheLookupDone(
      const PpdProvider::QueryAvailableCallback& done_callback,
      std::unique_ptr<base::Optional<PpdProvider::AvailablePrintersMap>>
          cache_result) {
    if (*cache_result) {
      done_callback.Run(PpdProvider::SUCCESS, cache_result->value());
      return;
    }

    auto fetch_data = base::MakeUnique<QueryAvailableFetchData>();
    fetch_data->done_callback = done_callback;

    fetch_data->fetcher = net::URLFetcher::Create(GetQuirksServerPpdListURL(),
                                                  net::URLFetcher::GET,
                                                  &query_available_delegate_);
    fetch_data->fetcher->SetRequestContext(url_context_getter_.get());
    fetch_data->fetcher->SetLoadFlags(
        net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
        net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
        net::LOAD_DO_NOT_SEND_AUTH_DATA);
    auto* fetcher = fetch_data->fetcher.get();
    StoreQueryAvailableFetchData(std::move(fetch_data));
    fetcher->Start();
  }

  void OnQueryAvailableFetchComplete(const net::URLFetcher* source) {
    std::unique_ptr<QueryAvailableFetchData> fetch_data =
        GetQueryAvailableFetchData(source);

    std::string contents;
    if (!ValidateAndGetResponseAsString(*source, &contents)) {
      // Something went wrong with the fetch.
      fetch_data->done_callback.Run(PpdProvider::SERVER_ERROR,
                                    AvailablePrintersMap());
      return;
    }

    // The server gives us JSON in the form of a list of (manufacturer, list of
    // models) tuples.
    auto top_dict =
        base::DictionaryValue::From(base::JSONReader::Read(contents));
    const base::ListValue* top_list;
    if (top_dict == nullptr || !top_dict->GetList(kJSONTopListKey, &top_list)) {
      LOG(ERROR) << "Malformed response from quirks server";
      fetch_data->done_callback.Run(PpdProvider::SERVER_ERROR,
                                    AvailablePrintersMap());
      return;
    }

    auto result = base::MakeUnique<PpdProvider::AvailablePrintersMap>();
    for (const std::unique_ptr<base::Value>& entry : *top_list) {
      base::DictionaryValue* dict;
      std::string manufacturer;
      std::string model;
      base::ListValue* model_list;
      if (!entry->GetAsDictionary(&dict) ||
          !dict->GetString(kJSONManufacturer, &manufacturer) ||
          !dict->GetList(kJSONModelList, &model_list)) {
        LOG(ERROR) << "Unexpected contents in quirks server printer list.";
        // Just skip this entry instead of aborting the whole thing.
        continue;
      }

      std::vector<std::string>& dest = (*result)[manufacturer];
      for (const std::unique_ptr<base::Value>& model_value : *model_list) {
        if (model_value->GetAsString(&model)) {
          dest.push_back(model);
        } else {
          LOG(ERROR) << "Skipping unknown model for manufacturer "
                     << manufacturer;
        }
      }
    }
    fetch_data->done_callback.Run(PpdProvider::SUCCESS, *result);
    if (!result->empty()) {
      cache_->StoreAvailablePrinters(std::move(result));
    } else {
      // An empty map means something is probably wrong; if we cache this map,
      // we'll have an empty map until the cache expires.  So complain and
      // refuse to cache.
      LOG(ERROR) << "Available printers map is unexpectedly empty.  Refusing "
                    "to cache this.";
    }
  }

 private:
  void StoreResolveFetchData(std::unique_ptr<ResolveFetchData> fetch_data) {
    auto raw_ptr = fetch_data->fetcher.get();
    base::AutoLock lock(resolve_fetches_lock_);
    resolve_fetches_[raw_ptr] = std::move(fetch_data);
  }

  void StoreQueryAvailableFetchData(
      std::unique_ptr<QueryAvailableFetchData> fetch_data) {
    auto raw_ptr = fetch_data->fetcher.get();
    base::AutoLock lock(query_available_fetches_lock_);
    query_available_fetches_[raw_ptr] = std::move(fetch_data);
  }

  // Extract the result of a resolve url fetch into |ppd_contents| and
  // |last_updated time|.  Returns true on success.
  bool ExtractResolveResponseData(const net::URLFetcher* source,
                                  const ResolveFetchData* fetch,
                                  std::string* ppd_contents,
                                  uint64_t* last_updated_time) {
    std::string contents;
    if (!ValidateAndGetResponseAsString(*source, &contents)) {
      LOG(WARNING) << "Response not validated";
      return false;
    }

    auto dict = base::DictionaryValue::From(base::JSONReader::Read(contents));
    if (dict == nullptr) {
      LOG(WARNING) << "Response not a dictionary";
      return false;
    }
    std::string last_updated_time_string;
    if (!dict->GetString(kJSONPPDKey, ppd_contents) ||
        !dict->GetString(kJSONLastUpdatedKey, &last_updated_time_string) ||
        !base::StringToUint64(last_updated_time_string, last_updated_time)) {
      // Malformed response.  TODO(justincarlson) - LOG something here?
      return false;
    }

    if (ppd_contents->size() > options_.max_ppd_contents_size_) {
      LOG(WARNING) << "PPD too large";
      // PPD is too big.
      //
      // Note -- if we ever add shared-ppd-sourcing, e.g. we may serve a ppd to
      // a user that's not from an explicitly trusted source, we should also
      // check *uncompressed* size here to head off zip-bombs (e.g. let's
      // compress 1GBs of zeros into a 900kb file and see what cups does when it
      // tries to expand that...)
      ppd_contents->clear();
      return false;
    }
    return base::Base64Decode(*ppd_contents, ppd_contents);
  }

  // Return the ResolveFetchData associated with |source|.
  std::unique_ptr<ResolveFetchData> GetResolveFetchData(
      const net::URLFetcher* source) {
    base::AutoLock l(resolve_fetches_lock_);
    auto it = resolve_fetches_.find(source);
    CHECK(it != resolve_fetches_.end());
    auto ret = std::move(it->second);
    resolve_fetches_.erase(it);
    return ret;
  }

  // Return the QueryAvailableFetchData associated with |source|.
  std::unique_ptr<QueryAvailableFetchData> GetQueryAvailableFetchData(
      const net::URLFetcher* source) {
    base::AutoLock lock(query_available_fetches_lock_);
    auto it = query_available_fetches_.find(source);
    CHECK(it != query_available_fetches_.end()) << "Fetch data not found!";
    auto fetch_data = std::move(it->second);
    query_available_fetches_.erase(it);
    return fetch_data;
  }

  // Trivial wrappers around PpdCache::Find() and
  // PpdCache::FindAvailablePrinters().  We need these wrappers both because we
  // use weak_ptrs to manage lifetime, and so we need to bind callbacks to
  // *this*, and because weak_ptr's preclude return values in posted tasks, so
  // we have to use a parameter to return state.
  void ResolveDoCacheLookup(
      const Printer::PpdReference& reference,
      base::Optional<base::FilePath>* cache_result) const {
    *cache_result = cache_->Find(reference);
  }

  void QueryAvailableDoCacheLookup(
      base::Optional<PpdProvider::AvailablePrintersMap>* cache_result) const {
    *cache_result = cache_->FindAvailablePrinters();
  }

  // Callback that happens when the Resolve() cache lookup completes.  If the
  // cache satisfied the request, finish the Resolve, otherwise start a URL
  // request to satisfy the request.  This runs on the same thread as Resolve()
  // was invoked on.
  void ResolveCacheLookupDone(
      const Printer::PpdReference& ppd_reference,
      const PpdProvider::ResolveCallback& done_callback,
      std::unique_ptr<base::Optional<base::FilePath>> cache_result) {
    if (*cache_result) {
      // Cache hit.  Schedule the callback now and return.
      done_callback.Run(PpdProvider::SUCCESS, cache_result->value());
      return;
    }

    // We don't have a way to automatically resolve user-supplied PPDs yet.  So
    // if we have one specified, and it's not cached, we fail out rather than
    // fall back to quirks-server based resolution.  The reasoning here is that
    // if the user has specified a PPD when a quirks-server one exists, it
    // probably means the quirks server one doesn't work for some reason, so we
    // shouldn't silently use it.
    if (!ppd_reference.user_supplied_ppd_url.empty()) {
      done_callback.Run(PpdProvider::NOT_FOUND, base::FilePath());
      return;
    }

    auto fetch_data = base::MakeUnique<ResolveFetchData>();
    fetch_data->ppd_reference = ppd_reference;
    fetch_data->done_callback = done_callback;
    fetch_data->fetcher =
        net::URLFetcher::Create(GetQuirksServerPpdLookupURL(ppd_reference),
                                net::URLFetcher::GET, &resolve_delegate_);

    fetch_data->fetcher->SetRequestContext(url_context_getter_.get());
    fetch_data->fetcher->SetLoadFlags(
        net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
        net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
        net::LOAD_DO_NOT_SEND_AUTH_DATA);
    auto* fetcher = fetch_data->fetcher.get();
    StoreResolveFetchData(std::move(fetch_data));
    fetcher->Start();
  }

  // Generate a url to look up a manufacturer/model from the quirks server
  GURL GetQuirksServerPpdLookupURL(
      const Printer::PpdReference& ppd_reference) const {
    return GURL(base::StringPrintf(
        "https://%s/v2/printer/manufacturers/%s/models/%s?key=%s",
        options_.quirks_server.c_str(),
        ppd_reference.effective_manufacturer.c_str(),
        ppd_reference.effective_model.c_str(), api_key_.c_str()));
  }

  // Generate a url to ask for the full supported printer list from the quirks
  // server.
  GURL GetQuirksServerPpdListURL() const {
    return GURL(base::StringPrintf("https://%s/v2/printer/list?key=%s",
                                   options_.quirks_server.c_str(),
                                   api_key_.c_str()));
  }

  // If |fetcher| succeeded in its fetch, get the response in |response| and
  // return true, otherwise return false.
  bool ValidateAndGetResponseAsString(const net::URLFetcher& fetcher,
                                      std::string* contents) {
    return (fetcher.GetStatus().status() == net::URLRequestStatus::SUCCESS) &&
           (fetcher.GetResponseCode() == net::HTTP_OK) &&
           fetcher.GetResponseAsString(contents);
  }

  // API key for accessing quirks server.
  const std::string api_key_;

  scoped_refptr<net::URLRequestContextGetter> url_context_getter_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  std::unique_ptr<PpdCache> cache_;

  // Construction-time options, immutable.
  const PpdProvider::Options options_;

  ResolveURLFetcherDelegate resolve_delegate_;
  QueryAvailableURLFetcherDelegate query_available_delegate_;

  // Active resolve fetches and associated lock.
  std::unordered_map<const net::URLFetcher*, std::unique_ptr<ResolveFetchData>>
      resolve_fetches_;
  base::Lock resolve_fetches_lock_;

  // Active QueryAvailable() fetches and associated lock.
  std::unordered_map<const net::URLFetcher*,
                     std::unique_ptr<QueryAvailableFetchData>>
      query_available_fetches_;
  base::Lock query_available_fetches_lock_;

  base::WeakPtrFactory<PpdProviderImpl> weak_factory_;
};

void ResolveURLFetcherDelegate::OnURLFetchComplete(
    const net::URLFetcher* source) {
  parent_->OnResolveFetchComplete(source);
}

void QueryAvailableURLFetcherDelegate::OnURLFetchComplete(
    const net::URLFetcher* source) {
  parent_->OnQueryAvailableFetchComplete(source);
}

}  // namespace

// static
std::unique_ptr<PpdProvider> PpdProvider::Create(
    const std::string& api_key,
    scoped_refptr<net::URLRequestContextGetter> url_context_getter,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    std::unique_ptr<PpdCache> cache,
    const PpdProvider::Options& options) {
  return base::MakeUnique<PpdProviderImpl>(
      api_key, url_context_getter, io_task_runner, std::move(cache), options);
}

}  // namespace printing
}  // namespace chromeos
