// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection_service.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
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

namespace {
static const int64 kDownloadRequestTimeoutMs = 3000;
}  // namespace

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

// Enumerate for histogramming purposes.
// DO NOT CHANGE THE ORDERING OF THESE VALUES (different histogram data will
// be mixed together based on their values).
enum SBStatsType {
  DOWNLOAD_URL_CHECKS_TOTAL,
  DOWNLOAD_URL_CHECKS_CANCELED,
  DOWNLOAD_URL_CHECKS_MALWARE,

  DOWNLOAD_HASH_CHECKS_TOTAL,
  DOWNLOAD_HASH_CHECKS_MALWARE,

  // Memory space for histograms is determined by the max.
  // ALWAYS ADD NEW VALUES BEFORE THIS ONE.
  DOWNLOAD_CHECKS_MAX
};
}  // namespace

DownloadProtectionService::DownloadInfo::DownloadInfo()
    : total_bytes(0), user_initiated(false) {}

DownloadProtectionService::DownloadInfo::~DownloadInfo() {}

std::string DownloadProtectionService::DownloadInfo::DebugString() const {
  std::string chain;
  for (size_t i = 0; i < download_url_chain.size(); ++i) {
    chain += download_url_chain[i].spec();
    if (i < download_url_chain.size() - 1) {
      chain += " -> ";
    }
  }
  return base::StringPrintf(
      "DownloadInfo {addr:0x%p, download_url_chain:[%s], local_file:%s, "
      "target_file:%s, referrer_url:%s, sha256_hash:%s, total_bytes:%" PRId64
      ", user_initiated: %s}",
      reinterpret_cast<const void*>(this),
      chain.c_str(),
      local_file.value().c_str(),
      target_file.value().c_str(),
      referrer_url.spec().c_str(),
      "TODO",
      total_bytes,
      user_initiated ? "true" : "false");
}

// static
DownloadProtectionService::DownloadInfo
DownloadProtectionService::DownloadInfo::FromDownloadItem(
    const DownloadItem& item) {
  DownloadInfo download_info;
  download_info.local_file = item.full_path();
  download_info.target_file = item.GetTargetFilePath();
  download_info.download_url_chain = item.url_chain();
  download_info.referrer_url = item.referrer_url();
  download_info.sha256_hash = item.hash();
  download_info.total_bytes = item.total_bytes();
  // TODO(bryner): Populate user_initiated
  return download_info;
}

// Parent SafeBrowsing::Client class used to lookup the bad binary
// URL and digest list.  There are two sub-classes (one for each list).
class DownloadSBClient
    : public SafeBrowsingService::Client,
      public base::RefCountedThreadSafe<DownloadSBClient> {
 public:
  DownloadSBClient(
      const DownloadProtectionService::DownloadInfo& info,
      const DownloadProtectionService::CheckDownloadCallback& callback,
      SafeBrowsingService* sb_service,
      SBStatsType total_type,
      SBStatsType dangerous_type)
      : info_(info),
        callback_(callback),
        sb_service_(sb_service),
        start_time_(base::TimeTicks::Now()),
        total_type_(total_type),
        dangerous_type_(dangerous_type) {}

  virtual void StartCheck() = 0;
  virtual bool IsDangerous(SafeBrowsingService::UrlCheckResult res) const = 0;

 protected:
  friend class base::RefCountedThreadSafe<DownloadSBClient>;
  virtual ~DownloadSBClient() {}

  void CheckDone(SafeBrowsingService::UrlCheckResult sb_result) {
    DownloadProtectionService::DownloadCheckResult result =
        IsDangerous(sb_result) ?
        DownloadProtectionService::DANGEROUS :
        DownloadProtectionService::SAFE;
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(callback_, result));
    UpdateDownloadCheckStats(total_type_);
    if (sb_result != SafeBrowsingService::SAFE) {
      UpdateDownloadCheckStats(dangerous_type_);
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&DownloadSBClient::ReportMalware,
                     this, sb_result));
    }
  }

  void ReportMalware(SafeBrowsingService::UrlCheckResult result) {
    std::string post_data;
    if (!info_.sha256_hash.empty())
      post_data += base::HexEncode(info_.sha256_hash.data(),
                                   info_.sha256_hash.size()) + "\n";
    for (size_t i = 0; i < info_.download_url_chain.size(); ++i) {
      post_data += info_.download_url_chain[i].spec() + "\n";
    }
    sb_service_->ReportSafeBrowsingHit(
        info_.download_url_chain.back(),  // malicious_url
        info_.download_url_chain.front(), // page_url
        info_.referrer_url,
        true,  // is_subresource
        result,
        post_data);
  }

  void UpdateDownloadCheckStats(SBStatsType stat_type) {
    UMA_HISTOGRAM_ENUMERATION("SB2.DownloadChecks",
                              stat_type,
                              DOWNLOAD_CHECKS_MAX);
  }

  DownloadProtectionService::DownloadInfo info_;
  DownloadProtectionService::CheckDownloadCallback callback_;
  scoped_refptr<SafeBrowsingService> sb_service_;
  base::TimeTicks start_time_;

 private:
  const SBStatsType total_type_;
  const SBStatsType dangerous_type_;

  DISALLOW_COPY_AND_ASSIGN(DownloadSBClient);
};

class DownloadUrlSBClient : public DownloadSBClient {
 public:
  DownloadUrlSBClient(
      const DownloadProtectionService::DownloadInfo& info,
      const DownloadProtectionService::CheckDownloadCallback& callback,
      SafeBrowsingService* sb_service)
      : DownloadSBClient(info, callback, sb_service,
                         DOWNLOAD_URL_CHECKS_TOTAL,
                         DOWNLOAD_URL_CHECKS_MALWARE) {}

  virtual void StartCheck() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!sb_service_ ||
        sb_service_->CheckDownloadUrl(info_.download_url_chain, this)) {
      CheckDone(SafeBrowsingService::SAFE);
    } else {
      AddRef();  // SafeBrowsingService takes a pointer not a scoped_refptr.
    }
  }

  virtual bool IsDangerous(
      SafeBrowsingService::UrlCheckResult result) const OVERRIDE {
    return result == SafeBrowsingService::BINARY_MALWARE_URL;
  }

  virtual void OnDownloadUrlCheckResult(
      const std::vector<GURL>& url_chain,
      SafeBrowsingService::UrlCheckResult sb_result) OVERRIDE {
    CheckDone(sb_result);
    UMA_HISTOGRAM_TIMES("SB2.DownloadUrlCheckDuration",
                        base::TimeTicks::Now() - start_time_);
    Release();
  }

 protected:
  virtual ~DownloadUrlSBClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadUrlSBClient);
};

class DownloadHashSBClient : public DownloadSBClient {
 public:
  DownloadHashSBClient(
      const DownloadProtectionService::DownloadInfo& info,
      const DownloadProtectionService::CheckDownloadCallback& callback,
      SafeBrowsingService* sb_service)
      : DownloadSBClient(info, callback, sb_service,
                         DOWNLOAD_HASH_CHECKS_TOTAL,
                         DOWNLOAD_HASH_CHECKS_MALWARE) {}

  virtual void StartCheck() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!sb_service_ ||
        sb_service_->CheckDownloadHash(info_.sha256_hash, this)) {
      CheckDone(SafeBrowsingService::SAFE);
    } else {
      AddRef();  // SafeBrowsingService takes a pointer not a scoped_refptr.
    }
  }

  virtual bool IsDangerous(
      SafeBrowsingService::UrlCheckResult result) const OVERRIDE {
    // We always return false here because we don't want to warn based on
    // a match with the digest list.  However, for UMA users, we want to
    // report the malware URL: DownloadSBClient::CheckDone() will still report
    // the URL even if the download is not considered dangerous.
    return false;
  }

  virtual void OnDownloadHashCheckResult(
      const std::string& hash,
      SafeBrowsingService::UrlCheckResult sb_result) OVERRIDE {
    CheckDone(sb_result);
    UMA_HISTOGRAM_TIMES("SB2.DownloadHashCheckDuration",
                        base::TimeTicks::Now() - start_time_);
    Release();
  }

 protected:
  virtual ~DownloadHashSBClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadHashSBClient);
};

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
        pingback_enabled_(service_->enabled()),
        finished_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(timeout_weakptr_factory_(this)),
        start_time_(base::TimeTicks::Now()) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  void Start() {
    VLOG(2) << "Starting SafeBrowsing download check for: "
            << info_.DebugString();
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // TODO(noelutz): implement some cache to make sure we don't issue the same
    // request over and over again if a user downloads the same binary multiple
    // times.
    if (info_.download_url_chain.empty()) {
      RecordImprovedProtectionStats(REASON_EMPTY_URL_CHAIN);
      PostFinishTask(SAFE);
      return;
    }
    const GURL& final_url = info_.download_url_chain.back();
    if (!final_url.is_valid() || final_url.is_empty()) {
      RecordImprovedProtectionStats(REASON_INVALID_URL);
      PostFinishTask(SAFE);
      return;
    }
    RecordFileExtensionType(info_.target_file);

    if (final_url.SchemeIs("https") || !IsBinaryFile(info_.target_file)) {
      RecordImprovedProtectionStats(final_url.SchemeIs("https") ?
                  REASON_HTTPS_URL : REASON_NOT_BINARY_FILE);
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(&CheckClientDownloadRequest::CheckDigestList, this));
      return;
    }

    // Compute features from the file contents. Note that we record histograms
    // based on the result, so this runs regardless of whether the pingbacks
    // are enabled.  Since we do blocking I/O, this happens on the file thread.
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&CheckClientDownloadRequest::ExtractFileFeatures, this));

    // If the request takes too long we cancel it.
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&CheckClientDownloadRequest::Cancel,
                   timeout_weakptr_factory_.GetWeakPtr()),
        service_->download_request_timeout_ms());
  }

  // Canceling a request will cause us to always report the result as SAFE
  // unless a pending request is about to call FinishRequest.
  void Cancel() {
    // Calling FinishRequest might delete this object if we don't keep a
    // reference around until Cancel() is finished running.
    scoped_refptr<CheckClientDownloadRequest> request(this);
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    FinishRequest(SAFE);
    if (fetcher_.get()) {
      // The DownloadProtectionService is going to release its reference, so we
      // might be destroyed before the URLFetcher completes.  Cancel the
      // fetcher so it does not try to invoke OnURLFetchComplete.
      fetcher_.reset();
    }
    // Note: If there is no fetcher, then some callback is still holding a
    // reference to this object.  We'll eventually wind up in some method on
    // the UI thread that will call FinishRequest() again.  If FinishRequest()
    // is called a second time, it will be a no-op.
    service_ = NULL;
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
    RecordImprovedProtectionStats(reason);
    UMA_HISTOGRAM_TIMES("SBClientDownload.DownloadRequestDuration",
                        base::TimeTicks::Now() - start_time_);
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
    bool is_signed = (signature_info_.certificate_chain_size() > 0);
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

  void CheckDigestList() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!sb_service_.get() || info_.sha256_hash.empty()) {
      PostFinishTask(SAFE);
    } else {
      scoped_refptr<DownloadSBClient> client(
          new DownloadHashSBClient(
              info_,
              base::Bind(&CheckClientDownloadRequest::FinishRequest, this),
              sb_service_.get()));
      // The client will release itself once it is done.
      client->StartCheck();
    }
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
      if (reason != REASON_MAX || signature_info_.trusted()) {
        UMA_HISTOGRAM_COUNTS("SBClientDownload.SignedOrWhitelistedDownload", 1);
      }
    }
    if (reason == REASON_MAX && signature_info_.trusted()) {
      // TODO(noelutz): implement a certificate whitelist and only whitelist
      // binaries whose certificate match the whitelist.
      reason = REASON_TRUSTED_EXECUTABLE;
    }
    if (reason != REASON_MAX) {
      RecordImprovedProtectionStats(reason);
      CheckDigestList();
    } else if (!pingback_enabled_) {
      RecordImprovedProtectionStats(REASON_PING_DISABLED);
      CheckDigestList();
    } else {
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
      RecordImprovedProtectionStats(REASON_REQUEST_CANCELED);
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
      RecordImprovedProtectionStats(REASON_INVALID_REQUEST_PROTO);
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
    fetcher_->SetAutomaticallyRetryOn5xx(false);  // Don't retry on error.
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
    if (finished_) {
      return;
    }
    finished_ = true;
    if (service_) {
      callback_.Run(result);
      service_->RequestFinished(this);
    } else {
      callback_.Run(SAFE);
    }
  }

  void RecordImprovedProtectionStats(DownloadCheckResultReason reason) {
    VLOG(2) << "SafeBrowsing download verdict for: "
            << info_.DebugString() << " verdict:" << reason;
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
  bool finished_;
  base::WeakPtrFactory<CheckClientDownloadRequest> timeout_weakptr_factory_;
  base::TimeTicks start_time_;  // Used for stats.

  DISALLOW_COPY_AND_ASSIGN(CheckClientDownloadRequest);
};

DownloadProtectionService::DownloadProtectionService(
    SafeBrowsingService* sb_service,
    net::URLRequestContextGetter* request_context_getter)
    : sb_service_(sb_service),
      request_context_getter_(request_context_getter),
      enabled_(false),
      signature_util_(new SignatureUtil()),
      download_request_timeout_ms_(kDownloadRequestTimeoutMs) {}

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

void DownloadProtectionService::CheckDownloadUrl(
    const DownloadProtectionService::DownloadInfo& info,
    const CheckDownloadCallback& callback) {
  DCHECK(!info.download_url_chain.empty());
  scoped_refptr<DownloadUrlSBClient> client(
      new DownloadUrlSBClient(info, callback, sb_service_));
  // The client will release itself once it is done.
  BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DownloadUrlSBClient::StartCheck, client));
}

bool DownloadProtectionService::IsSupportedFileType(
    const FilePath& filename) const {
  return IsBinaryFile(filename);
}

void DownloadProtectionService::CancelPendingRequests() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (std::set<scoped_refptr<CheckClientDownloadRequest> >::iterator it =
           download_requests_.begin();
       it != download_requests_.end();) {
    // We need to advance the iterator before we cancel because canceling
    // the request will invalidate it when RequestFinished is called below.
    scoped_refptr<CheckClientDownloadRequest> tmp = *it++;
    tmp->Cancel();
  }
  DCHECK(download_requests_.empty());
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
