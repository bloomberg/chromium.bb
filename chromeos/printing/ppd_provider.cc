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

using base::FilePath;
using net::URLFetcher;
using std::string;
using std::unique_ptr;

namespace chromeos {
namespace printing {
namespace {

// Expected fields from the quirks server.
const char kJSONPPDKey[] = "compressedPpd";
const char kJSONLastUpdatedKey[] = "lastUpdatedTime";

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
  void OnURLFetchComplete(const URLFetcher* source) override;

 private:
  // owner of this delegate.
  PpdProviderImpl* const owner_;
};

class PpdProviderImpl : public PpdProvider {
 public:
  PpdProviderImpl(
      const string& api_key,
      scoped_refptr<net::URLRequestContextGetter> url_context_getter,
      unique_ptr<PpdCache> cache,
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
               PpdProvider::ResolveCallback cb) override {
    CHECK(sequence_checker_.CalledOnValidSequence());
    CHECK(base::SequencedTaskRunnerHandle::IsSet())
        << "Resolve must be called from a SequencedTaskRunner context";

    CHECK(fetcher_ == nullptr)
        << "Can't have concurrent PpdProvider Resolve calls";

    base::Optional<FilePath> tmp = cache_->Find(ppd_reference);
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

    active_reference_ = ppd_reference;
    done_callback_ = cb;

    fetcher_ = URLFetcher::Create(GetQuirksServerLookupURL(ppd_reference),
                                  URLFetcher::GET, &forwarding_delegate_);
    fetcher_->SetRequestContext(url_context_getter_.get());
    fetcher_->SetLoadFlags(net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
                           net::LOAD_DO_NOT_SAVE_COOKIES |
                           net::LOAD_DO_NOT_SEND_COOKIES |
                           net::LOAD_DO_NOT_SEND_AUTH_DATA);
    fetcher_->Start();
  };

  void AbortResolve() override {
    // UrlFetcher guarantees that when the object has been destroyed, no further
    // callbacks will occur.
    fetcher_.reset();
  }

  bool CachePpd(const Printer::PpdReference& ppd_reference,
                const base::FilePath& ppd_path) override {
    string buf;
    if (!base::ReadFileToStringWithMaxSize(ppd_path, &buf,
                                           options_.max_ppd_contents_size_)) {
      return false;
    }
    return static_cast<bool>(cache_->Store(ppd_reference, buf));
  }

  // Called on the same thread as Resolve() when the fetcher completes its
  // fetch.
  void OnURLFetchComplete() {
    CHECK(sequence_checker_.CalledOnValidSequence());
    // Scope the allocated |fetcher_| into this function so we clean it up when
    // we're done here instead of leaving it around until the next Resolve call.
    auto fetcher = std::move(fetcher_);
    string contents;
    if ((fetcher->GetStatus().status() != net::URLRequestStatus::SUCCESS) ||
        (fetcher->GetResponseCode() != net::HTTP_OK) ||
        !fetcher->GetResponseAsString(&contents)) {
      // Something went wrong with the fetch.
      done_callback_.Run(PpdProvider::SERVER_ERROR, FilePath());
      return;
    }

    auto dict = base::DictionaryValue::From(base::JSONReader::Read(contents));
    if (dict == nullptr) {
      done_callback_.Run(PpdProvider::SERVER_ERROR, FilePath());
      return;
    }
    string ppd_contents;
    string last_updated_time_string;
    uint64_t last_updated_time;
    if (!(dict->GetString(kJSONPPDKey, &ppd_contents) &&
          dict->GetString(kJSONLastUpdatedKey, &last_updated_time_string) &&
          base::StringToUint64(last_updated_time_string, &last_updated_time))) {
      // Malformed response.  TODO(justincarlson) - LOG something here?
      done_callback_.Run(PpdProvider::SERVER_ERROR, FilePath());
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
      done_callback_.Run(PpdProvider::SERVER_ERROR, FilePath());
      return;
    }

    auto ppd_file = cache_->Store(active_reference_, ppd_contents);
    if (!ppd_file) {
      // Failed to store.
      done_callback_.Run(PpdProvider::INTERNAL_ERROR, FilePath());
      return;
    }
    done_callback_.Run(PpdProvider::SUCCESS, ppd_file.value());
  }

 private:
  // Generate a url to look up a manufacturer/model from the quirks server
  GURL GetQuirksServerLookupURL(
      const Printer::PpdReference& ppd_reference) const {
    return GURL(base::StringPrintf(
        "https://%s/v2/printer/manufacturers/%s/models/%s?key=%s",
        options_.quirks_server.c_str(),
        ppd_reference.effective_manufacturer.c_str(),
        ppd_reference.effective_model.c_str(), api_key_.c_str()));
  }

  // API key for accessing quirks server.
  const string api_key_;

  // Reference we're currently trying to resolve.
  Printer::PpdReference active_reference_;

  ForwardingURLFetcherDelegate forwarding_delegate_;
  scoped_refptr<net::URLRequestContextGetter> url_context_getter_;
  unique_ptr<PpdCache> cache_;

  PpdProvider::ResolveCallback done_callback_;

  // Check that Resolve() and its callback are sequenced appropriately.
  base::SequenceChecker sequence_checker_;

  // Fetcher for the current resolve call, if any.
  unique_ptr<URLFetcher> fetcher_;

  // Construction-time options, immutable.
  const PpdProvider::Options options_;
};

void ForwardingURLFetcherDelegate::OnURLFetchComplete(
    const URLFetcher* source) {
  owner_->OnURLFetchComplete();
}

}  // namespace

// static
unique_ptr<PpdProvider> PpdProvider::Create(
    const string& api_key,
    scoped_refptr<net::URLRequestContextGetter> url_context_getter,
    unique_ptr<PpdCache> cache,
    const PpdProvider::Options& options) {
  return base::MakeUnique<PpdProviderImpl>(api_key, url_context_getter,
                                           std::move(cache), options);
}

}  // namespace printing
}  // namespace chromeos
