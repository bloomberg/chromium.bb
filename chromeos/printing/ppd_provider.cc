// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_provider.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_parser.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
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

// URLFetcherDelegate that just invokes a callback when the fetch is complete.
class ForwardingURLFetcherDelegate : public net::URLFetcherDelegate {
 public:
  explicit ForwardingURLFetcherDelegate(
      const base::Callback<void()>& done_callback)
      : done_callback_(done_callback) {}
  ~ForwardingURLFetcherDelegate() override {}

  // URLFetcherDelegate API method.  Defined below since we need the
  // PpdProviderImpl definition first.
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    done_callback_.Run();
  }

 private:
  // Callback to be run on fetch complete.
  base::Callback<void()> done_callback_;
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
        weak_factory_(this) {
    CHECK_GT(options_.max_ppd_contents_size_, 0U);
  }
  ~PpdProviderImpl() override {}

  void Resolve(const Printer::PpdReference& ppd_reference,
               const PpdProvider::ResolveCallback& cb) override {
    CHECK(!cb.is_null());
    CHECK(resolve_sequence_checker_.CalledOnValidSequence());
    CHECK(base::SequencedTaskRunnerHandle::IsSet())
        << "Resolve must be called from a SequencedTaskRunner context";
    CHECK(!resolve_inflight_)
        << "Can't have concurrent PpdProvider Resolve calls";
    resolve_inflight_ = true;
    auto cache_result = base::MakeUnique<base::Optional<base::FilePath>>();
    auto* raw_cache_result_ptr = cache_result.get();
    bool post_result = io_task_runner_->PostTaskAndReply(
        FROM_HERE, base::Bind(&PpdProviderImpl::ResolveDoCacheLookup,
                              weak_factory_.GetWeakPtr(), ppd_reference,
                              raw_cache_result_ptr),
        base::Bind(&PpdProviderImpl::ResolveCacheLookupDone,
                   weak_factory_.GetWeakPtr(), ppd_reference, cb,
                   std::move(cache_result)));
    DCHECK(post_result);
  }

  void QueryAvailable(const QueryAvailableCallback& cb) override {
    CHECK(!cb.is_null());
    CHECK(base::SequencedTaskRunnerHandle::IsSet())
        << "QueryAvailable() must be called from a SequencedTaskRunner context";
    auto cache_result = base::MakeUnique<
        base::Optional<const PpdProvider::AvailablePrintersMap*>>();
    CHECK(!query_inflight_)
        << "Can't have concurrent PpdProvider QueryAvailable calls";
    query_inflight_ = true;
    auto* raw_cache_result_ptr = cache_result.get();
    CHECK(io_task_runner_->PostTaskAndReply(
        FROM_HERE, base::Bind(&PpdProviderImpl::QueryAvailableDoCacheLookup,
                              weak_factory_.GetWeakPtr(), raw_cache_result_ptr),
        base::Bind(&PpdProviderImpl::QueryAvailableCacheLookupDone,
                   weak_factory_.GetWeakPtr(), cb, std::move(cache_result))));
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

 private:
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
      base::Optional<const PpdProvider::AvailablePrintersMap*>* cache_result)
      const {
    DCHECK(cache_result);
    auto tmp = cache_->FindAvailablePrinters();
    if (tmp != nullptr) {
      *cache_result = tmp;
    } else {
      *cache_result = base::nullopt;
    }
  }

  // Callback that happens when the Resolve() cache lookup completes.  If the
  // cache satisfied the request, finish the Resolve, otherwise start a URL
  // request to satisfy the request.  This runs on the same thread as Resolve()
  // was invoked on.
  void ResolveCacheLookupDone(
      const Printer::PpdReference& ppd_reference,
      const PpdProvider::ResolveCallback& done_callback,
      const std::unique_ptr<base::Optional<base::FilePath>>& cache_result) {
    CHECK(resolve_sequence_checker_.CalledOnValidSequence());
    if (*cache_result) {
      // Cache hit.  Schedule the callback now and return.
      resolve_inflight_ = false;
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
      resolve_inflight_ = false;
      done_callback.Run(PpdProvider::NOT_FOUND, base::FilePath());
      return;
    }

    // Missed in the cache, so start a URLRequest to resolve the request.
    resolve_fetcher_delegate_ = base::MakeUnique<ForwardingURLFetcherDelegate>(
        base::Bind(&PpdProviderImpl::OnResolveFetchComplete,
                   weak_factory_.GetWeakPtr(), ppd_reference, done_callback));

    resolve_fetcher_ = net::URLFetcher::Create(
        GetQuirksServerPpdLookupURL(ppd_reference), net::URLFetcher::GET,
        resolve_fetcher_delegate_.get());

    resolve_fetcher_->SetRequestContext(url_context_getter_.get());
    resolve_fetcher_->SetLoadFlags(
        net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
        net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
        net::LOAD_DO_NOT_SEND_AUTH_DATA);
    resolve_fetcher_->Start();
  }

  // Called on the same thread as Resolve() when the fetcher completes its
  // fetch.
  void OnResolveFetchComplete(
      const Printer::PpdReference& ppd_reference,
      const PpdProvider::ResolveCallback& done_callback) {
    CHECK(resolve_sequence_checker_.CalledOnValidSequence());
    // Scope the allocated |resolve_fetcher_| and |resolve_fetcher_delegate_|
    // into this function so we clean it up when we're done here instead of
    // leaving it around until the next Resolve() call.
    auto fetcher_delegate(std::move(resolve_fetcher_delegate_));
    auto fetcher(std::move(resolve_fetcher_));
    resolve_inflight_ = false;
    std::string contents;
    if (!ValidateAndGetResponseAsString(*fetcher, &contents)) {
      // Something went wrong with the fetch.
      done_callback.Run(PpdProvider::SERVER_ERROR, base::FilePath());
      return;
    }

    auto dict = base::DictionaryValue::From(base::JSONReader::Read(contents));
    if (dict == nullptr) {
      done_callback.Run(PpdProvider::SERVER_ERROR, base::FilePath());
      return;
    }
    std::string ppd_contents;
    std::string last_updated_time_string;
    uint64_t last_updated_time;
    if (!(dict->GetString(kJSONPPDKey, &ppd_contents) &&
          dict->GetString(kJSONLastUpdatedKey, &last_updated_time_string) &&
          base::StringToUint64(last_updated_time_string, &last_updated_time))) {
      // Malformed response.  TODO(justincarlson) - LOG something here?
      done_callback.Run(PpdProvider::SERVER_ERROR, base::FilePath());
      return;
    }

    if (ppd_contents.size() > options_.max_ppd_contents_size_) {
      // PPD is too big.
      //
      // Note -- if we ever add shared-ppd-sourcing, e.g. we may serve a ppd to
      // a user that's not from an explicitly trusted source, we should also
      // check *uncompressed* size here to head off zip-bombs (e.g. let's
      // compress 1GBs of zeros into a 900kb file and see what cups does when it
      // tries to expand that...)
      done_callback.Run(PpdProvider::SERVER_ERROR, base::FilePath());
      return;
    }

    auto ppd_file = cache_->Store(ppd_reference, ppd_contents);
    if (!ppd_file) {
      // Failed to store.
      done_callback.Run(PpdProvider::INTERNAL_ERROR, base::FilePath());
      return;
    }
    done_callback.Run(PpdProvider::SUCCESS, ppd_file.value());
  }

  // Called on the same thread as QueryAvailable() when the cache lookup is
  // done.  If the cache satisfied the request, finish the Query, otherwise
  // start a URL request to satisfy the Query.  This runs on the same thread as
  // QueryAvailable() was invoked on.
  void QueryAvailableCacheLookupDone(
      const PpdProvider::QueryAvailableCallback& done_callback,
      const std::unique_ptr<
          base::Optional<const PpdProvider::AvailablePrintersMap*>>&
          cache_result) {
    CHECK(query_sequence_checker_.CalledOnValidSequence());
    if (*cache_result) {
      query_inflight_ = false;
      done_callback.Run(PpdProvider::SUCCESS, *cache_result->value());
      return;
    }
    // Missed in the cache, start a query.
    query_fetcher_delegate_ = base::MakeUnique<ForwardingURLFetcherDelegate>(
        base::Bind(&PpdProviderImpl::OnQueryAvailableFetchComplete,
                   weak_factory_.GetWeakPtr(), done_callback));

    query_fetcher_ = net::URLFetcher::Create(GetQuirksServerPpdListURL(),
                                             net::URLFetcher::GET,
                                             query_fetcher_delegate_.get());
    query_fetcher_->SetRequestContext(url_context_getter_.get());
    query_fetcher_->SetLoadFlags(
        net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
        net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
        net::LOAD_DO_NOT_SEND_AUTH_DATA);
    query_fetcher_->Start();
  }

  void OnQueryAvailableFetchComplete(
      const PpdProvider::QueryAvailableCallback& done_callback) {
    CHECK(query_sequence_checker_.CalledOnValidSequence());
    // Scope the object fetcher and task_runner into this function so we clean
    // it up when we're done here instead of leaving it around until the next
    // QueryAvailable() call.
    auto fetcher_delegate(std::move(query_fetcher_delegate_));
    auto fetcher(std::move(query_fetcher_));
    query_inflight_ = false;
    std::string contents;
    if (!ValidateAndGetResponseAsString(*fetcher, &contents)) {
      // Something went wrong with the fetch.
      done_callback.Run(PpdProvider::SERVER_ERROR, AvailablePrintersMap());
      return;
    }

    // The server gives us JSON in the form of a list of (manufacturer, list of
    // models) tuples.
    auto top_dict =
        base::DictionaryValue::From(base::JSONReader::Read(contents));
    const base::ListValue* top_list;
    if (top_dict == nullptr || !top_dict->GetList(kJSONTopListKey, &top_list)) {
      LOG(ERROR) << "Malformed response from quirks server";
      done_callback.Run(PpdProvider::SERVER_ERROR, AvailablePrintersMap());
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
    done_callback.Run(PpdProvider::SUCCESS, *result);
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
    return ((fetcher.GetStatus().status() == net::URLRequestStatus::SUCCESS) &&
            (fetcher.GetResponseCode() == net::HTTP_OK) &&
            fetcher.GetResponseAsString(contents));
  }

  // State held across a Resolve() call.

  // Check that Resolve() and its callback are sequenced appropriately.
  base::SequenceChecker resolve_sequence_checker_;

  // Fetcher and associated delegate for the current Resolve() call, if a fetch
  // is in progress.  These are both null if no Resolve() is in flight.
  std::unique_ptr<net::URLFetcher> resolve_fetcher_;
  std::unique_ptr<ForwardingURLFetcherDelegate> resolve_fetcher_delegate_;
  // Is there a current Resolve() inflight?  Used to help fail-fast in the case
  // of inappropriate concurrent usage.
  bool resolve_inflight_ = false;

  // State held across a QueryAvailable() call.

  // Check that QueryAvailable() and its callback are sequenced appropriately.
  base::SequenceChecker query_sequence_checker_;

  // Fetcher and associated delegate for the current QueryAvailable() call, if a
  // fetch is in progress.  These are both null if no QueryAvailable() is in
  // flight.
  std::unique_ptr<net::URLFetcher> query_fetcher_;
  std::unique_ptr<ForwardingURLFetcherDelegate> query_fetcher_delegate_;
  // Is there a current QueryAvailable() inflight?  Used to help fail-fast in
  // the case of inappropriate concurrent usage.
  bool query_inflight_ = false;

  // Common state.

  // API key for accessing quirks server.
  const std::string api_key_;

  scoped_refptr<net::URLRequestContextGetter> url_context_getter_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  std::unique_ptr<PpdCache> cache_;

  // Construction-time options, immutable.
  const PpdProvider::Options options_;

  base::WeakPtrFactory<PpdProviderImpl> weak_factory_;
};

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
