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
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/webapk.pb.h"
#include "chrome/browser/android/webapk/webapk_icon_hasher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/manifest_util.h"
#include "jni/WebApkInstaller_jni.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

namespace {

// The default WebAPK server URL.
const char kDefaultWebApkServerUrl[] =
    "https://webapk.googleapis.com/v1alpha/webApks/";

// The response format type expected from the WebAPK server.
const char kDefaultWebApkServerUrlResponseType[] = "?alt=proto";

// The MIME type of the POST data sent to the server.
const char kProtoMimeType[] = "application/x-protobuf";

// The default number of milliseconds to wait for the WebAPK download URL from
// the WebAPK server.
const int kWebApkDownloadUrlTimeoutMs = 60000;

// The default number of milliseconds to wait for the WebAPK download to
// complete.
const int kDownloadTimeoutMs = 60000;

// Returns the scope from |info| if it is specified. Otherwise, returns the
// default scope.
GURL GetScope(const ShortcutInfo& info) {
  return (info.scope.is_valid())
      ? info.scope
      : ShortcutHelper::GetScopeFromURL(info.url);
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

// Populates webapk::WebApk and returns it.
// Must be called on a worker thread because it encodes an SkBitmap.
std::unique_ptr<webapk::WebApk> BuildWebApkProtoInBackground(
    const ShortcutInfo& shortcut_info,
    const SkBitmap& shortcut_icon,
    const std::string& shortcut_icon_murmur2_hash) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  std::unique_ptr<webapk::WebApk> webapk(new webapk::WebApk);
  webapk->set_manifest_url(shortcut_info.manifest_url.spec());
  webapk->set_requester_application_package(
      base::android::BuildInfo::GetInstance()->package_name());
  webapk->set_requester_application_version(version_info::GetVersionNumber());

  webapk::WebAppManifest* web_app_manifest = webapk->mutable_manifest();
  web_app_manifest->set_name(base::UTF16ToUTF8(shortcut_info.name));
  web_app_manifest->set_short_name(
      base::UTF16ToUTF8(shortcut_info.short_name));
  web_app_manifest->set_start_url(shortcut_info.url.spec());
  web_app_manifest->set_orientation(
      content::WebScreenOrientationLockTypeToString(
          shortcut_info.orientation));
  web_app_manifest->set_display_mode(
      content::WebDisplayModeToString(shortcut_info.display));
  web_app_manifest->set_background_color(
      ColorToString(shortcut_info.background_color));
  web_app_manifest->set_theme_color(ColorToString(shortcut_info.theme_color));

  std::string* scope = web_app_manifest->add_scopes();
  scope->assign(GetScope(shortcut_info).spec());
  webapk::Image* image = web_app_manifest->add_icons();
  image->set_src(shortcut_info.icon_url.spec());
  image->set_hash(shortcut_icon_murmur2_hash);
  std::vector<unsigned char> png_bytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(shortcut_icon, false, &png_bytes);
  image->set_image_data(&png_bytes.front(), png_bytes.size());

  return webapk;
}

// Returns task runner for running background tasks.
scoped_refptr<base::TaskRunner> GetBackgroundTaskRunner() {
  return content::BrowserThread::GetBlockingPool()
      ->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

GURL GetServerUrlForUpdate(const GURL& server_url,
                           const std::string& webapk_package) {
  // crbug.com/636552. Simplify the server URL.
  return GURL(server_url.spec() + webapk_package + "/" +
              kDefaultWebApkServerUrlResponseType);
}

}  // anonymous namespace

WebApkInstaller::WebApkInstaller(const ShortcutInfo& shortcut_info,
                                 const SkBitmap& shortcut_icon)
    : shortcut_info_(shortcut_info),
      shortcut_icon_(shortcut_icon),
      webapk_download_url_timeout_ms_(kWebApkDownloadUrlTimeoutMs),
      download_timeout_ms_(kDownloadTimeoutMs),
      task_type_(UNDEFINED),
      weak_ptr_factory_(this) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  server_url_ =
      GURL(command_line->HasSwitch(switches::kWebApkServerUrl)
               ? command_line->GetSwitchValueASCII(switches::kWebApkServerUrl)
               : kDefaultWebApkServerUrl);
}

WebApkInstaller::~WebApkInstaller() {}

void WebApkInstaller::InstallAsync(content::BrowserContext* browser_context,
                                   const FinishCallback& finish_callback) {
  InstallAsyncWithURLRequestContextGetter(
      Profile::FromBrowserContext(browser_context)->GetRequestContext(),
      finish_callback);
}

void WebApkInstaller::InstallAsyncWithURLRequestContextGetter(
    net::URLRequestContextGetter* request_context_getter,
    const FinishCallback& finish_callback) {
  request_context_getter_ = request_context_getter;
  finish_callback_ = finish_callback;
  task_type_ = INSTALL;

  if (!shortcut_info_.icon_url.is_valid()) {
    OnFailure();
    return;
  }

  // We need to take the hash of the bitmap at the icon URL prior to any
  // transformations being applied to the bitmap (such as encoding/decoding
  // the bitmap). The icon hash is used to determine whether the icon that
  // the user sees matches the icon of a WebAPK that the WebAPK server
  // generated for another user. (The icon can be dynamically generated.)
  //
  // We redownload the icon in order to take the Murmur2 hash. The redownload
  // should be fast because the icon should be in the HTTP cache.
  DownloadAppIconAndComputeMurmur2Hash();
}

void WebApkInstaller::SetTimeoutMs(int timeout_ms) {
  webapk_download_url_timeout_ms_ = timeout_ms;
  download_timeout_ms_ = timeout_ms;
}

void WebApkInstaller::UpdateAsync(content::BrowserContext* browser_context,
                                  const FinishCallback& finish_callback,
                                  const std::string& webapk_package,
                                  int webapk_version) {
  UpdateAsyncWithURLRequestContextGetter(
      Profile::FromBrowserContext(browser_context)->GetRequestContext(),
      finish_callback, webapk_package, webapk_version);
}

void WebApkInstaller::UpdateAsyncWithURLRequestContextGetter(
    net::URLRequestContextGetter* request_context_getter,
    const FinishCallback& finish_callback,
    const std::string& webapk_package,
    int webapk_version) {
  request_context_getter_ = request_context_getter;
  finish_callback_ = finish_callback;
  webapk_package_ = webapk_package;
  webapk_version_ = webapk_version;
  task_type_ = UPDATE;

  if (!shortcut_info_.icon_url.is_valid()) {
    OnFailure();
    return;
  }

  DownloadAppIconAndComputeMurmur2Hash();
}

bool WebApkInstaller::StartInstallingDownloadedWebApk(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jstring>& java_file_path,
    const base::android::ScopedJavaLocalRef<jstring>& java_package_name) {
  return Java_WebApkInstaller_installAsyncFromNative(env, java_file_path,
                                                     java_package_name);
}

bool WebApkInstaller::StartUpdateUsingDownloadedWebApk(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jstring>& java_file_path,
    const base::android::ScopedJavaLocalRef<jstring>& java_package_name) {
  return Java_WebApkInstaller_updateAsyncFromNative(env, java_file_path,
                                                    java_package_name);
}

void WebApkInstaller::OnURLFetchComplete(const net::URLFetcher* source) {
  timer_.Stop();

  if (!source->GetStatus().is_success() ||
      source->GetResponseCode() != net::HTTP_OK) {
    OnFailure();
    return;
  }

  std::string response_string;
  source->GetResponseAsString(&response_string);

  std::unique_ptr<webapk::WebApkResponse> response(
      new webapk::WebApkResponse);
  if (!response->ParseFromString(response_string)) {
    OnFailure();
    return;
  }

  GURL signed_download_url(response->signed_download_url());
  if (!signed_download_url.is_valid() || response->package_name().empty()) {
    OnFailure();
    return;
  }
  OnGotWebApkDownloadUrl(signed_download_url, response->package_name());
}

void WebApkInstaller::DownloadAppIconAndComputeMurmur2Hash() {
  timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(download_timeout_ms_),
      base::Bind(&WebApkInstaller::OnTimeout, weak_ptr_factory_.GetWeakPtr()));

  icon_hasher_.reset(new WebApkIconHasher());
  icon_hasher_->DownloadAndComputeMurmur2Hash(
      request_context_getter_, shortcut_info_.icon_url,
      base::Bind(&WebApkInstaller::OnGotIconMurmur2Hash,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstaller::OnGotIconMurmur2Hash(
    const std::string& icon_murmur2_hash) {
  timer_.Stop();
  icon_hasher_.reset();

  shortcut_icon_murmur2_hash_ = icon_murmur2_hash;

  // An empty hash indicates that |icon_hasher_| encountered an error.
  if (icon_murmur2_hash.empty()) {
    OnFailure();
    return;
  }

  if (task_type_ == INSTALL) {
    base::PostTaskAndReplyWithResult(
        GetBackgroundTaskRunner().get(), FROM_HERE,
        base::Bind(&BuildWebApkProtoInBackground, shortcut_info_,
                   shortcut_icon_, shortcut_icon_murmur2_hash_),
        base::Bind(&WebApkInstaller::SendCreateWebApkRequest,
                   weak_ptr_factory_.GetWeakPtr()));
  } else if (task_type_ == UPDATE) {
    base::PostTaskAndReplyWithResult(
        GetBackgroundTaskRunner().get(), FROM_HERE,
        base::Bind(&BuildWebApkProtoInBackground, shortcut_info_,
                   shortcut_icon_, shortcut_icon_murmur2_hash_),
        base::Bind(&WebApkInstaller::SendUpdateWebApkRequest,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void WebApkInstaller::SendCreateWebApkRequest(
    std::unique_ptr<webapk::WebApk> webapk) {
  GURL server_url(server_url_.spec() + kDefaultWebApkServerUrlResponseType);
  SendRequest(std::move(webapk), net::URLFetcher::POST, server_url);
}

void WebApkInstaller::SendUpdateWebApkRequest(
    std::unique_ptr<webapk::WebApk> webapk) {
  webapk->set_package_name(webapk_package_);
  webapk->set_version(std::to_string(webapk_version_));

  SendRequest(std::move(webapk), net::URLFetcher::PUT,
              GetServerUrlForUpdate(server_url_, webapk_package_));
}

void WebApkInstaller::SendRequest(std::unique_ptr<webapk::WebApk> request_proto,
                                  net::URLFetcher::RequestType request_type,
                                  const GURL& server_url) {
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(webapk_download_url_timeout_ms_),
      base::Bind(&WebApkInstaller::OnTimeout, weak_ptr_factory_.GetWeakPtr()));

  url_fetcher_ = net::URLFetcher::Create(server_url, request_type, this);
  url_fetcher_->SetRequestContext(request_context_getter_);
  std::string serialized_request;
  request_proto->SerializeToString(&serialized_request);
  url_fetcher_->SetUploadData(kProtoMimeType, serialized_request);
  url_fetcher_->Start();
}

void WebApkInstaller::OnGotWebApkDownloadUrl(const GURL& download_url,
                                             const std::string& package_name) {
  base::FilePath output_dir;
  base::android::GetCacheDirectory(&output_dir);
  // TODO(pkotwicz): Download WebAPKs into WebAPK-specific subdirectory
  // directory.
  // TODO(pkotwicz): Figure out when downloaded WebAPK should be deleted.

  timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(download_timeout_ms_),
      base::Bind(&WebApkInstaller::OnTimeout, weak_ptr_factory_.GetWeakPtr()));

  base::FilePath output_path = output_dir.AppendASCII(package_name);
  downloader_.reset(new FileDownloader(
      download_url, output_path, true, request_context_getter_,
      base::Bind(&WebApkInstaller::OnWebApkDownloaded,
                 weak_ptr_factory_.GetWeakPtr(), output_path, package_name)));
}

void WebApkInstaller::OnWebApkDownloaded(const base::FilePath& file_path,
                                         const std::string& package_name,
                                         FileDownloader::Result result) {
  timer_.Stop();

  if (result != FileDownloader::DOWNLOADED) {
    OnFailure();
    return;
  }

  int posix_permissions = base::FILE_PERMISSION_READ_BY_USER |
                          base::FILE_PERMISSION_WRITE_BY_USER |
                          base::FILE_PERMISSION_READ_BY_GROUP |
                          base::FILE_PERMISSION_READ_BY_OTHERS;
  base::PostTaskAndReplyWithResult(
      GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&base::SetPosixFilePermissions, file_path, posix_permissions),
      base::Bind(&WebApkInstaller::OnWebApkMadeWorldReadable,
                 weak_ptr_factory_.GetWeakPtr(), file_path, package_name));
}

void WebApkInstaller::OnWebApkMadeWorldReadable(
    const base::FilePath& file_path,
    const std::string& package_name,
    bool change_permission_success) {
  if (!change_permission_success) {
    OnFailure();
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_file_path =
      base::android::ConvertUTF8ToJavaString(env, file_path.value());
  base::android::ScopedJavaLocalRef<jstring> java_package_name =
      base::android::ConvertUTF8ToJavaString(env, package_name);
  bool success = false;
  if (task_type_ == INSTALL) {
    success = StartInstallingDownloadedWebApk(env, java_file_path,
                                              java_package_name);
  } else if (task_type_ == UPDATE) {
    success = StartUpdateUsingDownloadedWebApk(env, java_file_path,
                                               java_package_name);
  }
  if (success)
    OnSuccess();
  else
    OnFailure();
}

void WebApkInstaller::OnTimeout() {
  OnFailure();
}

void WebApkInstaller::OnSuccess() {
  FinishCallback callback = finish_callback_;
  delete this;
  callback.Run(true);
}

void WebApkInstaller::OnFailure() {
  FinishCallback callback = finish_callback_;
  delete this;
  callback.Run(false);
}
