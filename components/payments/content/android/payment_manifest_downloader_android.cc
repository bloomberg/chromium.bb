// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_manifest_downloader_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/payments/content/payment_manifest_downloader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "jni/PaymentManifestDownloader_jni.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace payments {
namespace {

class SelfDeletingDownloadDelegate
    : public PaymentManifestDownloader::Delegate {
 public:
  explicit SelfDeletingDownloadDelegate(
      const base::android::JavaParamRef<jobject>& jcallback)
      : jcallback_(jcallback), is_downloading_payment_method_manifest_(true) {}

  void set_downloader(std::unique_ptr<PaymentManifestDownloader> downloader) {
    downloader_ = std::move(downloader);
  }

  void DownloadPaymentMethodManifest() {
    is_downloading_payment_method_manifest_ = true;
    downloader_->DownloadPaymentMethodManifest();
  }

  void DownloadWebAppManifest() {
    is_downloading_payment_method_manifest_ = false;
    downloader_->DownloadWebAppManifest();
  }

  // PaymentManifestDownloader::Delegate
  void OnManifestDownloadSuccess(const std::string& content) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    if (is_downloading_payment_method_manifest_) {
      Java_ManifestDownloadCallback_onPaymentMethodManifestDownloadSuccess(
          env, jcallback_,
          base::android::ConvertUTF8ToJavaString(env, content));
    } else {
      Java_ManifestDownloadCallback_onWebAppManifestDownloadSuccess(
          env, jcallback_,
          base::android::ConvertUTF8ToJavaString(env, content));
    }
    delete this;
  }

  // PaymentManifestDownloader::Delegate
  void OnManifestDownloadFailure() override {
    Java_ManifestDownloadCallback_onManifestDownloadFailure(
        base::android::AttachCurrentThread(), jcallback_);
    delete this;
  }

 private:
  ~SelfDeletingDownloadDelegate() override {}

  base::android::ScopedJavaGlobalRef<jobject> jcallback_;
  bool is_downloading_payment_method_manifest_;
  std::unique_ptr<PaymentManifestDownloader> downloader_;

  DISALLOW_COPY_AND_ASSIGN(SelfDeletingDownloadDelegate);
};

SelfDeletingDownloadDelegate* BuildSelfDeletingDownloadDelegate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jobject>& juri,
    const base::android::JavaParamRef<jobject>& jcallback) {
  SelfDeletingDownloadDelegate* delegate =
      new SelfDeletingDownloadDelegate(jcallback);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents) {
    delegate->OnManifestDownloadFailure();
    return nullptr;
  }

  GURL url(base::android::ConvertJavaStringToUTF8(
      env, Java_PaymentManifestDownloader_getUriString(env, juri)));
  DCHECK(url.is_valid());
  DCHECK(url.SchemeIs(url::kHttpsScheme));

  std::unique_ptr<PaymentManifestDownloader> downloader =
      base::MakeUnique<PaymentManifestDownloader>(
          content::BrowserContext::GetDefaultStoragePartition(
              web_contents->GetBrowserContext())
              ->GetURLRequestContext(),
          url, delegate);
  delegate->set_downloader(std::move(downloader));

  return delegate;
}

}  // namespace

bool RegisterPaymentManifestDownloader(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void DownloadPaymentMethodManifest(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jobject>& jmethod_name,
    const base::android::JavaParamRef<jobject>& jcallback) {
  SelfDeletingDownloadDelegate* delegate = BuildSelfDeletingDownloadDelegate(
      env, jweb_contents, jmethod_name, jcallback);
  if (delegate)
    delegate->DownloadPaymentMethodManifest();
}

void DownloadWebAppManifest(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    const base::android::JavaParamRef<jobject>& jweb_app_manifest_uri,
    const base::android::JavaParamRef<jobject>& jcallback) {
  SelfDeletingDownloadDelegate* delegate = BuildSelfDeletingDownloadDelegate(
      env, jweb_contents, jweb_app_manifest_uri, jcallback);
  if (delegate)
    delegate->DownloadWebAppManifest();
}

}  // namespace payments
