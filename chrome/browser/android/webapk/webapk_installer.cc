// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_installer.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/webapk.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "jni/WebApkInstaller_jni.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "third_party/smhasher/src/MurmurHash2.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

namespace {

// The default WebAPK server URL.
const char kDefaultWebApkServerUrl[] =
    "https://webapk.googleapis.com/v1alpha/webApks?alt=proto";

// The MIME type of the POST data sent to the server.
const char kProtoMimeType[] = "application/x-protobuf";

// The seed to use the murmur2 hash of the app icon.
const uint32_t kMurmur2HashSeed = 0;

// The number of milliseconds to wait for the WebAPK download URL from the
// WebAPK server.
const int kWebApkDownloadUrlTimeoutMs = 4000;

// The number of milliseconds to wait for the WebAPK download to complete.
const int kDownloadTimeoutMs = 20000;

// Returns the scope from |info| if it is specified. Otherwise, returns the
// default scope.
GURL GetScope(const ShortcutInfo& info) {
  return (info.scope.is_valid())
      ? info.scope
      : ShortcutHelper::GetScopeFromURL(info.url);
}

// Computes a murmur2 hash of |bitmap|'s PNG encoded bytes.
uint64_t ComputeBitmapHash(const SkBitmap& bitmap) {
  std::vector<unsigned char> png_bytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &png_bytes);
  return MurmurHash64B(&png_bytes.front(), png_bytes.size(), kMurmur2HashSeed);
}

// Converts a color from the format specified in content::Manifest to a CSS
// string.
std::string ColorToString(int64_t color) {
  if (color == content::Manifest::kInvalidOrMissingColor)
    return "";

  SkColor sk_color = reinterpret_cast<uint32_t&>(color);
  int r = SkColorGetR(sk_color);
  int g = SkColorGetG(sk_color);
  int b = SkColorGetB(sk_color);
  double a = SkColorGetA(sk_color) / 255.0;
  return base::StringPrintf("rgba(%d,%d,%d,%.2f)", r, g, b, a);
}

}  // anonymous namespace

WebApkInstaller::WebApkInstaller(const ShortcutInfo& shortcut_info,
                                 const SkBitmap& shortcut_icon)
    : shortcut_info_(shortcut_info),
      shortcut_icon_(shortcut_icon),
      io_weak_ptr_factory_(this) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  server_url_ =
      GURL(command_line->HasSwitch(switches::kWebApkServerUrl)
               ? command_line->GetSwitchValueASCII(switches::kWebApkServerUrl)
               : kDefaultWebApkServerUrl);
}

WebApkInstaller::~WebApkInstaller() {}

void WebApkInstaller::InstallAsync(content::BrowserContext* browser_context,
                                   const FinishCallback& finish_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  finish_callback_ = finish_callback;
  // base::Unretained() is safe because WebApkInstaller owns itself and does not
  // start the timeout timer till after
  // InitializeRequestContextGetterOnUIThread() is called.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&WebApkInstaller::InitializeRequestContextGetterOnUIThread,
                 base::Unretained(this), browser_context));
}

void WebApkInstaller::InstallAsyncWithURLRequestContextGetter(
    net::URLRequestContextGetter* request_context_getter,
    const FinishCallback& finish_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  request_context_getter_ = request_context_getter;
  finish_callback_ = finish_callback;

  SendCreateWebApkRequest();
}

bool WebApkInstaller::StartDownloadedWebApkInstall(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jstring>& java_file_path,
    const base::android::ScopedJavaLocalRef<jstring>& java_package_name) {
  return Java_WebApkInstaller_installAsyncFromNative(env, java_file_path.obj(),
                                                     java_package_name.obj());
}

void WebApkInstaller::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  timer_.Stop();

  if (!source->GetStatus().is_success() ||
      source->GetResponseCode() != net::HTTP_OK) {
    OnFailure();
    return;
  }

  std::string response_string;
  source->GetResponseAsString(&response_string);

  std::unique_ptr<webapk::CreateWebApkResponse> response(
      new webapk::CreateWebApkResponse);
  if (!response->ParseFromString(response_string)) {
    OnFailure();
    return;
  }

  if (response->signed_download_url().empty() ||
      response->webapk_package_name().empty()) {
    OnFailure();
    return;
  }
  OnGotWebApkDownloadUrl(response->signed_download_url(),
                         response->webapk_package_name());
}

void WebApkInstaller::InitializeRequestContextGetterOnUIThread(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Profile::GetRequestContext() must be called on UI thread.
  request_context_getter_ =
      Profile::FromBrowserContext(browser_context)->GetRequestContext();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&WebApkInstaller::SendCreateWebApkRequest,
                 io_weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstaller::SendCreateWebApkRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::unique_ptr<webapk::CreateWebApkRequest> request =
      BuildCreateWebApkRequest();

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kWebApkDownloadUrlTimeoutMs),
               base::Bind(&WebApkInstaller::OnTimeout,
                          io_weak_ptr_factory_.GetWeakPtr()));

  url_fetcher_ =
      net::URLFetcher::Create(server_url_, net::URLFetcher::POST, this);
  url_fetcher_->SetRequestContext(request_context_getter_);
  std::string serialized_request;
  request->SerializeToString(&serialized_request);
  url_fetcher_->SetUploadData(kProtoMimeType, serialized_request);
  url_fetcher_->Start();
}

void WebApkInstaller::OnGotWebApkDownloadUrl(const std::string& download_url,
                                             const std::string& package_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::FilePath output_dir;
  base::android::GetCacheDirectory(&output_dir);
  // TODO(pkotwicz): Download WebAPKs into WebAPK-specific subdirectory
  // directory.
  // TODO(pkotwicz): Figure out when downloaded WebAPK should be deleted.

  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kDownloadTimeoutMs),
               base::Bind(&WebApkInstaller::OnTimeout,
                          io_weak_ptr_factory_.GetWeakPtr()));

  base::FilePath output_path = output_dir.AppendASCII(package_name);
  downloader_.reset(new FileDownloader(
      GURL(download_url), output_path, true, request_context_getter_,
      base::Bind(&WebApkInstaller::OnWebApkDownloaded,
                 io_weak_ptr_factory_.GetWeakPtr(), output_path,
                 package_name)));
}

void WebApkInstaller::OnWebApkDownloaded(const base::FilePath& file_path,
                                         const std::string& package_name,
                                         FileDownloader::Result result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  timer_.Stop();

  if (result != FileDownloader::DOWNLOADED) {
    OnFailure();
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_file_path =
      base::android::ConvertUTF8ToJavaString(env, file_path.value());
  base::android::ScopedJavaLocalRef<jstring> java_package_name =
      base::android::ConvertUTF8ToJavaString(env, package_name);
  bool success =
      StartDownloadedWebApkInstall(env, java_file_path, java_package_name);
  if (success)
    OnSuccess();
  else
    OnFailure();
}

std::unique_ptr<webapk::CreateWebApkRequest>
WebApkInstaller::BuildCreateWebApkRequest() {
  std::unique_ptr<webapk::CreateWebApkRequest> request(
      new webapk::CreateWebApkRequest);

  webapk::WebApk* webapk = request->mutable_webapk();
  webapk->set_manifest_url(shortcut_info_.manifest_url.spec());
  webapk->set_requester_application_package(
      base::android::BuildInfo::GetInstance()->package_name());
  webapk->set_requester_application_version(version_info::GetVersionNumber());

  webapk::WebAppManifest* web_app_manifest = webapk->mutable_manifest();
  web_app_manifest->set_name(base::UTF16ToUTF8(shortcut_info_.name));
  web_app_manifest->set_short_name(
      base::UTF16ToUTF8(shortcut_info_.short_name));
  web_app_manifest->set_start_url(shortcut_info_.url.spec());
  // TODO(pkotwicz): Add "display mode" and "orientation" to proto.
  web_app_manifest->set_background_color(
      ColorToString(shortcut_info_.background_color));
  web_app_manifest->set_theme_color(ColorToString(shortcut_info_.theme_color));

  std::string* scope = web_app_manifest->add_scopes();
  scope->assign(GetScope(shortcut_info_).spec());
  webapk::Image* image = web_app_manifest->add_icons();
  image->set_src(shortcut_info_.icon_url.spec());
  // TODO(pkotwicz): Get Murmur2 hash of untransformed icon's bytes (with no
  // encoding/decoding).
  image->set_hash(ComputeBitmapHash(shortcut_icon_));
  std::vector<unsigned char> png_bytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(shortcut_icon_, false, &png_bytes);
  image->set_image_data(&png_bytes.front(), png_bytes.size());

  return request;
}

void WebApkInstaller::OnTimeout() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  OnFailure();
}

void WebApkInstaller::OnSuccess() {
  finish_callback_.Run(true);
  delete this;
}

void WebApkInstaller::OnFailure() {
  finish_callback_.Run(false);
  delete this;
}
