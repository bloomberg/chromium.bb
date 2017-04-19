// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_icon_hasher.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/data_url.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/smhasher/src/MurmurHash2.h"

namespace {

// The seed to use when taking the murmur2 hash of the icon.
const uint64_t kMurmur2HashSeed = 0;

// The default number of milliseconds to wait for the icon download to complete.
const int kDownloadTimeoutInMilliseconds = 60000;

// Computes Murmur2 hash of |raw_image_data|.
std::string ComputeMurmur2Hash(const std::string& raw_image_data) {
  // WARNING: We are running in the browser process. |raw_image_data| is the
  // image's raw, unsanitized bytes from the web. |raw_image_data| may contain
  // malicious data. Decoding unsanitized bitmap data to an SkBitmap in the
  // browser process is a security bug.
  uint64_t hash = MurmurHash64A(&raw_image_data.front(), raw_image_data.size(),
                                kMurmur2HashSeed);
  return base::Uint64ToString(hash);
}

}  // anonymous namespace

// static
void WebApkIconHasher::DownloadAndComputeMurmur2Hash(
    net::URLRequestContextGetter* request_context_getter,
    const GURL& icon_url,
    const Murmur2HashCallback& callback) {
  DownloadAndComputeMurmur2HashWithTimeout(request_context_getter, icon_url,
                                           kDownloadTimeoutInMilliseconds,
                                           callback);
}

// static
void WebApkIconHasher::DownloadAndComputeMurmur2HashWithTimeout(
    net::URLRequestContextGetter* request_context_getter,
    const GURL& icon_url,
    int timeout_ms,
    const Murmur2HashCallback& callback) {
  if (!icon_url.is_valid()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, ""));
    return;
  }

  if (icon_url.SchemeIs(url::kDataScheme)) {
    std::string mime_type, char_set, data;
    std::string hash;
    if (net::DataURL::Parse(icon_url, &mime_type, &char_set, &data) &&
        !data.empty()) {
      hash = ComputeMurmur2Hash(data);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, hash));
    return;
  }

  // The icon hasher will delete itself when it is done.
  new WebApkIconHasher(request_context_getter, icon_url, timeout_ms, callback);
}

WebApkIconHasher::WebApkIconHasher(
    net::URLRequestContextGetter* url_request_context_getter,
    const GURL& icon_url,
    int timeout_ms,
    const Murmur2HashCallback& callback)
    : callback_(callback) {
  download_timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(timeout_ms),
      base::Bind(&WebApkIconHasher::OnDownloadTimedOut,
                 base::Unretained(this)));

  url_fetcher_ = net::URLFetcher::Create(icon_url, net::URLFetcher::GET, this);
  url_fetcher_->SetRequestContext(url_request_context_getter);
  url_fetcher_->Start();
}

WebApkIconHasher::~WebApkIconHasher() {}

void WebApkIconHasher::OnURLFetchComplete(const net::URLFetcher* source) {
  download_timeout_timer_.Stop();

  if (!source->GetStatus().is_success() ||
      source->GetResponseCode() != net::HTTP_OK) {
    RunCallback("");
    return;
  }

  // WARNING: We are running in the browser process. |raw_image_data| is the
  // image's raw, unsanitized bytes from the web. |raw_image_data| may contain
  // malicious data. Decoding unsanitized bitmap data to an SkBitmap in the
  // browser process is a security bug.
  std::string raw_image_data;
  source->GetResponseAsString(&raw_image_data);
  RunCallback(ComputeMurmur2Hash(raw_image_data));
}

void WebApkIconHasher::OnDownloadTimedOut() {
  url_fetcher_.reset();

  RunCallback("");
}

void WebApkIconHasher::RunCallback(const std::string& icon_murmur2_hash) {
  callback_.Run(icon_murmur2_hash);
  delete this;
}
