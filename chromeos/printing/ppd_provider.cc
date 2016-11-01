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

// URLFetcherDelegate that just forwards the complete callback back to
// the PpdProvider that owns the delegate.
class ForwardingURLFetcherDelegate : public net::URLFetcherDelegate {
 public:
  explicit ForwardingURLFetcherDelegate(PpdProviderImpl* owner)
      : owner_(owner) {}
  ~ForwardingURLFetcherDelegate() override {}

  // URLFetcherDelegate API method.  Defined below since we need the
  // PpdProviderImpl definition first.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 private:
  // owner of this delegate.
  PpdProviderImpl* const owner_;
};

class PpdProviderImpl : public PpdProvider {
 public:
  PpdProviderImpl(
      const std::string& api_key,
      scoped_refptr<net::URLRequestContextGetter> url_context_getter,
      std::unique_ptr<PpdCache> cache,
      const PpdProvider::Options& options)
      : api_key_(api_key),
        forwarding_delegate_(this),
        url_context_getter_(url_context_getter),
        cache_(std::move(cache)),
        options_(options) {
    CHECK_GT(options_.max_ppd_contents_size_, 0U);
  }
  ~PpdProviderImpl() override {}

  void Resolve(const Printer::PpdReference& ppd_reference,
               const PpdProvider::ResolveCallback& cb) override {
    CHECK(!cb.is_null());
    CHECK(resolve_sequence_checker_.CalledOnValidSequence());
    CHECK(base::SequencedTaskRunnerHandle::IsSet())
        << "Resolve must be called from a SequencedTaskRunner context";
    CHECK(resolve_fetcher_ == nullptr)
        << "Can't have concurrent PpdProvider Resolve calls";

    base::Optional<base::FilePath> tmp = cache_->Find(ppd_reference);
    if (tmp) {
      // Cache hit.  Schedule the callback now and return.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(cb, PpdProvider::SUCCESS, tmp.value()));
      return;
    }

    // We don't have a way to automatically resolve user-supplied PPDs yet.  So
    // if we have one specified, and it's not cached, we fail out rather than
    // fall back to quirks-server based resolution.  The reasoning here is that
    // if the user has specified a PPD when a quirks-server one exists, it
    // probably means the quirks server one doesn't work for some reason, so we
    // shouldn't silently use it.
    if (!ppd_reference.user_supplied_ppd_url.empty()) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(cb, PpdProvider::NOT_FOUND, base::FilePath()));
      return;
    }

    resolve_reference_ = ppd_reference;
    resolve_done_callback_ = cb;

    resolve_fetcher_ =
        net::URLFetcher::Create(GetQuirksServerPpdLookupURL(ppd_reference),
                                net::URLFetcher::GET, &forwarding_delegate_);
    resolve_fetcher_->SetRequestContext(url_context_getter_.get());
    resolve_fetcher_->SetLoadFlags(
        net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
        net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
        net::LOAD_DO_NOT_SEND_AUTH_DATA);
    resolve_fetcher_->Start();
  };

  void AbortResolve() override {
    // UrlFetcher guarantees that when the object has been destroyed, no further
    // callbacks will occur.
    resolve_fetcher_.reset();
  }

  void QueryAvailable(const QueryAvailableCallback& cb) override {
    CHECK(!cb.is_null());
    CHECK(query_sequence_checker_.CalledOnValidSequence());
    CHECK(base::SequencedTaskRunnerHandle::IsSet())
        << "QueryAvailable() must be called from a SequencedTaskRunner context";
    CHECK(query_fetcher_ == nullptr)
        << "Can't have concurrent PpdProvider QueryAvailable() calls";

    base::Optional<PpdProvider::AvailablePrintersMap> result =
        cache_->FindAvailablePrinters();
    if (result) {
      // Satisfy from cache.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(cb, PpdProvider::SUCCESS, result.value()));
      return;
    }
    // Not in the cache, ask QuirksServer.
    query_done_callback_ = cb;

    query_fetcher_ =
        net::URLFetcher::Create(GetQuirksServerPpdListURL(),
                                net::URLFetcher::GET, &forwarding_delegate_);
    query_fetcher_->SetRequestContext(url_context_getter_.get());
    query_fetcher_->SetLoadFlags(
        net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
        net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
        net::LOAD_DO_NOT_SEND_AUTH_DATA);
    query_fetcher_->Start();
  }

  void AbortQueryAvailable() override {
    // UrlFetcher guarantees that when the object has been destroyed, no further
    // callbacks will occur.
    query_fetcher_.reset();
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

  // Route to the proper fetch complete handler based on which fetcher caused
  // it.
  void OnURLFetchComplete(const net::URLFetcher* fetcher) {
    if (fetcher == resolve_fetcher_.get()) {
      OnResolveFetchComplete();
    } else if (fetcher == query_fetcher_.get()) {
      OnQueryAvailableFetchComplete();
    } else {
      NOTREACHED() << "Unknown fetcher completed.";
    }
  }

 private:
  // Called on the same thread as Resolve() when the fetcher completes its
  // fetch.
  void OnResolveFetchComplete() {
    CHECK(resolve_sequence_checker_.CalledOnValidSequence());
    // Scope the allocated |resolve_fetcher_| into this function so we clean it
    // up when we're done here instead of leaving it around until the next
    // Resolve() call.
    auto fetcher = std::move(resolve_fetcher_);
    std::string contents;
    if (!ValidateAndGetResponseAsString(*fetcher, &contents)) {
      // Something went wrong with the fetch.
      resolve_done_callback_.Run(PpdProvider::SERVER_ERROR, base::FilePath());
      return;
    }

    auto dict = base::DictionaryValue::From(base::JSONReader::Read(contents));
    if (dict == nullptr) {
      resolve_done_callback_.Run(PpdProvider::SERVER_ERROR, base::FilePath());
      return;
    }
    std::string ppd_contents;
    std::string last_updated_time_string;
    uint64_t last_updated_time;
    if (!(dict->GetString(kJSONPPDKey, &ppd_contents) &&
          dict->GetString(kJSONLastUpdatedKey, &last_updated_time_string) &&
          base::StringToUint64(last_updated_time_string, &last_updated_time))) {
      // Malformed response.  TODO(justincarlson) - LOG something here?
      resolve_done_callback_.Run(PpdProvider::SERVER_ERROR, base::FilePath());
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
      resolve_done_callback_.Run(PpdProvider::SERVER_ERROR, base::FilePath());
      return;
    }

    auto ppd_file = cache_->Store(resolve_reference_, ppd_contents);
    if (!ppd_file) {
      // Failed to store.
      resolve_done_callback_.Run(PpdProvider::INTERNAL_ERROR, base::FilePath());
      return;
    }
    resolve_done_callback_.Run(PpdProvider::SUCCESS, ppd_file.value());
  }

  // Called on the same thread as QueryAvailable() when the fetcher completes
  // its fetch.
  void OnQueryAvailableFetchComplete() {
    CHECK(query_sequence_checker_.CalledOnValidSequence());
    // Scope the object fetcher into this function so we clean it up when we're
    // done here instead of leaving it around until the next QueryAvailable()
    // call.
    auto fetcher = std::move(query_fetcher_);
    std::string contents;
    if (!ValidateAndGetResponseAsString(*fetcher, &contents)) {
      // Something went wrong with the fetch.
      query_done_callback_.Run(PpdProvider::SERVER_ERROR,
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
      query_done_callback_.Run(PpdProvider::SERVER_ERROR,
                               PpdProvider::AvailablePrintersMap());
      return;
    }

    PpdProvider::AvailablePrintersMap result;
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

      std::vector<std::string>& dest = result[manufacturer];
      for (const std::unique_ptr<base::Value>& model_value : *model_list) {
        if (model_value->GetAsString(&model)) {
          dest.push_back(model);
        } else {
          LOG(ERROR) << "Skipping unknown model for manufacturer "
                     << manufacturer;
        }
      }
    }
    if (!result.empty()) {
      cache_->StoreAvailablePrinters(result);
    } else {
      // An empty map means something is probably wrong; if we cache this map,
      // we'll have an empty map until the cache expires.  So complain and
      // refuse to cache.
      LOG(ERROR) << "Available printers map is unexpectedly empty.  Refusing "
                    "to cache this.";
    }
    query_done_callback_.Run(PpdProvider::SUCCESS, result);
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

  // Reference we're currently trying to resolve.
  Printer::PpdReference resolve_reference_;

  // Callback to invoke on completion.
  PpdProvider::ResolveCallback resolve_done_callback_;

  // Check that Resolve() and its callback are sequenced appropriately.
  base::SequenceChecker resolve_sequence_checker_;

  // Fetcher for the current call, if any.
  std::unique_ptr<net::URLFetcher> resolve_fetcher_;

  // State held across a QueryAvailable() call.

  // Callback to invoke on completion.
  PpdProvider::QueryAvailableCallback query_done_callback_;

  // Check that QueryAvailable() and its callback are sequenced appropriately.
  base::SequenceChecker query_sequence_checker_;

  // Fetcher for the current call, if any.
  std::unique_ptr<net::URLFetcher> query_fetcher_;

  // Common state.

  // API key for accessing quirks server.
  const std::string api_key_;

  ForwardingURLFetcherDelegate forwarding_delegate_;
  scoped_refptr<net::URLRequestContextGetter> url_context_getter_;
  std::unique_ptr<PpdCache> cache_;

  // Construction-time options, immutable.
  const PpdProvider::Options options_;
};

void ForwardingURLFetcherDelegate::OnURLFetchComplete(
    const net::URLFetcher* source) {
  owner_->OnURLFetchComplete(source);
}

}  // namespace

// static
std::unique_ptr<PpdProvider> PpdProvider::Create(
    const std::string& api_key,
    scoped_refptr<net::URLRequestContextGetter> url_context_getter,
    std::unique_ptr<PpdCache> cache,
    const PpdProvider::Options& options) {
  return base::MakeUnique<PpdProviderImpl>(api_key, url_context_getter,
                                           std::move(cache), options);
}

}  // namespace printing
}  // namespace chromeos
