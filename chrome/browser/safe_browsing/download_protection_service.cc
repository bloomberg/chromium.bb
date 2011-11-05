// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection_service.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/signature_util.h"
#include "chrome/common/net/http_return.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/browser/download/download_item.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_fetcher.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

namespace safe_browsing {

const char DownloadProtectionService::kDownloadRequestUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/download";

namespace {
bool IsBinaryFile(const FilePath& file) {
  return (file.MatchesExtension(FILE_PATH_LITERAL(".exe")) ||
          file.MatchesExtension(FILE_PATH_LITERAL(".cab")) ||
          file.MatchesExtension(FILE_PATH_LITERAL(".msi")));
}

// List of extensions for which we track some UMA stats.
enum MaliciousExtensionType {
  EXTENSION_EXE,
  EXTENSION_MSI,
  EXTENSION_CAB,
  EXTENSION_SYS,
  EXTENSION_SCR,
  EXTENSION_DRV,
  EXTENSION_BAT,
  EXTENSION_ZIP,
  EXTENSION_RAR,
  EXTENSION_DLL,
  EXTENSION_PIF,
  EXTENSION_COM,
  EXTENSION_JAR,
  EXTENSION_CLASS,
  EXTENSION_PDF,
  EXTENSION_VB,
  EXTENSION_REG,
  EXTENSION_GRP,
  EXTENSION_OTHER,  // Groups all other extensions into one bucket.
  EXTENSION_MAX,
};

MaliciousExtensionType GetExtensionType(const FilePath& f) {
  if (f.MatchesExtension(FILE_PATH_LITERAL(".exe"))) return EXTENSION_EXE;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".msi"))) return EXTENSION_MSI;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".cab"))) return EXTENSION_CAB;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".sys"))) return EXTENSION_SYS;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".scr"))) return EXTENSION_SCR;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".drv"))) return EXTENSION_DRV;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".bat"))) return EXTENSION_BAT;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".zip"))) return EXTENSION_ZIP;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".rar"))) return EXTENSION_RAR;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".dll"))) return EXTENSION_DLL;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".pif"))) return EXTENSION_PIF;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".com"))) return EXTENSION_COM;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".jar"))) return EXTENSION_JAR;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".class"))) return EXTENSION_CLASS;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".pdf"))) return EXTENSION_PDF;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".vb"))) return EXTENSION_VB;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".reg"))) return EXTENSION_REG;
  if (f.MatchesExtension(FILE_PATH_LITERAL(".grp"))) return EXTENSION_GRP;
  return EXTENSION_OTHER;
}

void RecordFileExtensionType(const FilePath& file) {
  UMA_HISTOGRAM_ENUMERATION("SBClientDownload.DownloadExtensions",
                            GetExtensionType(file),
                            EXTENSION_MAX);
}
}  // namespace

DownloadProtectionService::DownloadInfo::DownloadInfo()
    : total_bytes(0), user_initiated(false) {}

DownloadProtectionService::DownloadInfo::~DownloadInfo() {}

// static
DownloadProtectionService::DownloadInfo
DownloadProtectionService::DownloadInfo::FromDownloadItem(
    const DownloadItem& item) {
  DownloadInfo download_info;
  download_info.local_file = item.full_path();
  download_info.download_url_chain = item.url_chain();
  download_info.referrer_url = item.referrer_url();
  // TODO(bryner): Fill in the hash (we shouldn't compute it again)
  download_info.total_bytes = item.total_bytes();
  // TODO(bryner): Populate user_initiated
  return download_info;
}

class DownloadProtectionService::CheckClientDownloadRequest
    : public base::RefCountedThreadSafe<
          DownloadProtectionService::CheckClientDownloadRequest,
          BrowserThread::DeleteOnUIThread>,
      public content::URLFetcherDelegate {
 public:
  CheckClientDownloadRequest(const DownloadInfo& info,
                             const CheckDownloadCallback& callback,
                             DownloadProtectionService* service,
                             SafeBrowsingService* sb_service,
                             SignatureUtil* signature_util)
      : info_(info),
        callback_(callback),
        service_(service),
        signature_util_(signature_util),
        sb_service_(sb_service),
        pingback_enabled_(service_->enabled()) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  void Start() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // TODO(noelutz): implement some cache to make sure we don't issue the same
    // request over and over again if a user downloads the same binary multiple
    // times.
    if (info_.download_url_chain.empty()) {
      RecordStats(REASON_INVALID_URL);
      PostFinishTask(SAFE);
      return;
    }
    const GURL& final_url = info_.download_url_chain.back();
    if (!final_url.is_valid() || final_url.is_empty() ||
        !final_url.SchemeIs("http")) {
      RecordStats(REASON_INVALID_URL);
      PostFinishTask(SAFE);
      return;  // For now we only support HTTP download URLs.
    }
    RecordFileExtensionType(info_.local_file);

    if (!IsBinaryFile(info_.local_file)) {
      RecordStats(REASON_NOT_BINARY_FILE);
      PostFinishTask(SAFE);
      return;
    }

    // Compute features from the file contents. Note that we record histograms
    // based on the result, so this runs regardless of whether the pingbacks
    // are enabled.  Since we do blocking I/O, this happens on the file thread.
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&CheckClientDownloadRequest::ExtractFileFeatures, this));
  }

  // Canceling a request will cause us to always report the result as SAFE.
  // In addition, the DownloadProtectionService will not be notified when the
  // request finishes, so it must drop its reference after calling Cancel.
  void Cancel() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    service_ = NULL;
    if (fetcher_.get()) {
      // The DownloadProtectionService is going to release its reference, so we
      // might be destroyed before the URLFetcher completes.  Cancel the
      // fetcher so it does not try to invoke OnURLFetchComplete.
      FinishRequest(SAFE);
      fetcher_.reset();
    }
    // Note: If there is no fetcher, then some callback is still holding a
    // reference to this object.  We'll eventually wind up in some method on
    // the UI thread that will call FinishRequest() and run the callback.
  }

  // From the content::URLFetcherDelegate interface.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK_EQ(source, fetcher_.get());
    VLOG(2) << "Received a response for URL: "
            << info_.download_url_chain.back() << ": success="
            << source->GetStatus().is_success() << " response_code="
            << source->GetResponseCode();
    DownloadCheckResultReason reason = REASON_SERVER_PING_FAILED;
    DownloadCheckResult result = SAFE;
    if (source->GetStatus().is_success() &&
        RC_REQUEST_OK == source->GetResponseCode()) {
      ClientDownloadResponse response;
      std::string data;
      bool got_data = source->GetResponseAsString(&data);
      DCHECK(got_data);
      if (!response.ParseFromString(data)) {
        reason = REASON_INVALID_RESPONSE_PROTO;
      } else if (response.verdict() == ClientDownloadResponse::DANGEROUS) {
        reason = REASON_DOWNLOAD_DANGEROUS;
        result = DANGEROUS;
      } else {
        reason = REASON_DOWNLOAD_SAFE;
      }
    }
    // We don't need the fetcher anymore.
    fetcher_.reset();
    RecordStats(reason);
    FinishRequest(result);
  }

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class DeleteTask<CheckClientDownloadRequest>;

  virtual ~CheckClientDownloadRequest() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  void ExtractFileFeatures() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    signature_util_->CheckSignature(info_.local_file, &signature_info_);
    bool is_signed = signature_info_.has_certificate_contents();
    if (is_signed) {
      VLOG(2) << "Downloaded a signed binary: " << info_.local_file.value();
    } else {
      VLOG(2) << "Downloaded an unsigned binary: " << info_.local_file.value();
    }
    UMA_HISTOGRAM_BOOLEAN("SBClientDownload.SignedBinaryDownload", is_signed);

    // TODO(noelutz): DownloadInfo should also contain the IP address of every
    // URL in the redirect chain.  We also should check whether the download
    // URL is hosted on the internal network.
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&CheckClientDownloadRequest::CheckWhitelists, this));
  }

  void CheckWhitelists() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DownloadCheckResultReason reason = REASON_MAX;
    if (!sb_service_.get()) {
      reason = REASON_SB_DISABLED;
    } else {
      for (size_t i = 0; i < info_.download_url_chain.size(); ++i) {
        const GURL& url = info_.download_url_chain[i];
        if (url.is_valid() && sb_service_->MatchDownloadWhitelistUrl(url)) {
          reason = REASON_WHITELISTED_URL;
          break;
        }
      }
      if (info_.referrer_url.is_valid() && reason == REASON_MAX &&
          sb_service_->MatchDownloadWhitelistUrl(info_.referrer_url)) {
        reason = REASON_WHITELISTED_REFERRER;
      }
      if (reason != REASON_MAX || signature_info_.has_certificate_contents()) {
        UMA_HISTOGRAM_COUNTS("SBClientDownload.SignedOrWhitelistedDownload", 1);
      }
    }
    if (reason != REASON_MAX) {
      RecordStats(reason);
      PostFinishTask(SAFE);
    } else if (!pingback_enabled_) {
      RecordStats(REASON_SB_DISABLED);
      PostFinishTask(SAFE);
    } else {
      // TODO(noelutz): check signature and CA against whitelist.

      // The URLFetcher is owned by the UI thread, so post a message to
      // start the pingback.
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&CheckClientDownloadRequest::SendRequest, this));
    }
  }

  void SendRequest() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    // This is our last chance to check whether the request has been canceled
    // before sending it.
    if (!service_) {
      RecordStats(REASON_REQUEST_CANCELED);
      FinishRequest(SAFE);
      return;
    }

    ClientDownloadRequest request;
    request.set_url(info_.download_url_chain.back().spec());
    request.mutable_digests()->set_sha256(info_.sha256_hash);
    request.set_length(info_.total_bytes);
    for (size_t i = 0; i < info_.download_url_chain.size(); ++i) {
      ClientDownloadRequest::Resource* resource = request.add_resources();
      resource->set_url(info_.download_url_chain[i].spec());
      if (i == info_.download_url_chain.size() - 1) {
        // The last URL in the chain is the download URL.
        resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);
        resource->set_referrer(info_.referrer_url.spec());
      } else {
        resource->set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);
      }
      // TODO(noelutz): fill out the remote IP addresses.
    }
    request.set_user_initiated(info_.user_initiated);
    request.mutable_signature()->CopyFrom(signature_info_);
    std::string request_data;
    if (!request.SerializeToString(&request_data)) {
      RecordStats(REASON_INVALID_REQUEST_PROTO);
      FinishRequest(SAFE);
      return;
    }

    VLOG(2) << "Sending a request for URL: "
            << info_.download_url_chain.back();
    fetcher_.reset(content::URLFetcher::Create(0 /* ID used for testing */,
                                               GURL(kDownloadRequestUrl),
                                               content::URLFetcher::POST,
                                               this));
    fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
    fetcher_->SetRequestContext(service_->request_context_getter_.get());
    fetcher_->SetUploadData("application/octet-stream", request_data);
    fetcher_->Start();
  }

  void PostFinishTask(DownloadCheckResult result) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&CheckClientDownloadRequest::FinishRequest, this, result));
  }

  void FinishRequest(DownloadCheckResult result) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (service_) {
      callback_.Run(result);
      service_->RequestFinished(this);
    } else {
      callback_.Run(SAFE);
    }
  }

  void RecordStats(DownloadCheckResultReason reason) {
    UMA_HISTOGRAM_ENUMERATION("SBClientDownload.CheckDownloadStats",
                              reason,
                              REASON_MAX);
  }

  DownloadInfo info_;
  ClientDownloadRequest_SignatureInfo signature_info_;
  CheckDownloadCallback callback_;
  // Will be NULL if the request has been canceled.
  DownloadProtectionService* service_;
  scoped_refptr<SignatureUtil> signature_util_;
  scoped_refptr<SafeBrowsingService> sb_service_;
  const bool pingback_enabled_;
  scoped_ptr<content::URLFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(CheckClientDownloadRequest);
};

DownloadProtectionService::DownloadProtectionService(
    SafeBrowsingService* sb_service,
    net::URLRequestContextGetter* request_context_getter)
    : sb_service_(sb_service),
      request_context_getter_(request_context_getter),
      enabled_(false),
      signature_util_(new SignatureUtil()) {}

DownloadProtectionService::~DownloadProtectionService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CancelPendingRequests();
}

void DownloadProtectionService::SetEnabled(bool enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (enabled == enabled_) {
    return;
  }
  enabled_ = enabled;
  if (!enabled_) {
    CancelPendingRequests();
  }
}

void DownloadProtectionService::CheckClientDownload(
    const DownloadProtectionService::DownloadInfo& info,
    const CheckDownloadCallback& callback) {
  scoped_refptr<CheckClientDownloadRequest> request(
      new CheckClientDownloadRequest(info, callback, this,
                                     sb_service_, signature_util_.get()));
  download_requests_.insert(request);
  request->Start();
}

void DownloadProtectionService::CancelPendingRequests() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (std::set<scoped_refptr<CheckClientDownloadRequest> >::iterator it =
           download_requests_.begin();
       it != download_requests_.end(); ++it) {
    (*it)->Cancel();
  }
  download_requests_.clear();
}

void DownloadProtectionService::RequestFinished(
    CheckClientDownloadRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::set<scoped_refptr<CheckClientDownloadRequest> >::iterator it =
      download_requests_.find(request);
  DCHECK(it != download_requests_.end());
  download_requests_.erase(*it);
}

}  // namespace safe_browsing
