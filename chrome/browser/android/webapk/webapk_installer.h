// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_

#include <jni.h>
#include <map>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace base {
class ElapsedTimer;
}

namespace content {
class BrowserContext;
}

namespace webapk {
class WebApk;
}

// Talks to Chrome WebAPK server to download metadata about a WebApk and issue
// a request for it to be installed. The native WebApkInstaller owns the Java
// WebApkInstaller counterpart.
class WebApkInstaller : public net::URLFetcherDelegate {
 public:
  using FinishCallback = WebApkInstallService::FinishCallback;

  ~WebApkInstaller() override;

  // Creates a self-owned WebApkInstaller instance and talks to the Chrome
  // WebAPK server to generate a WebAPK on the server and locally requests the
  // APK to be installed. Calls |callback| once the install completed or failed.
  static void InstallAsync(content::BrowserContext* context,
                           const ShortcutInfo& shortcut_info,
                           const SkBitmap& primary_icon,
                           const SkBitmap& badge_icon,
                           const FinishCallback& finish_callback);

  // Creates a self-owned WebApkInstaller instance and talks to the Chrome
  // WebAPK server to update a WebAPK on the server and locally requests the
  // APK to be installed. Calls |callback| once the install completed or failed.
  static void UpdateAsync(
      content::BrowserContext* context,
      const ShortcutInfo& shortcut_info,
      const SkBitmap& primary_icon,
      const SkBitmap& badge_icon,
      const std::string& webapk_package,
      int webapk_version,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale,
      const FinishCallback& callback);

  // Calls the private function |InstallAsync| for testing.
  // Should be used only for testing.
  static void InstallAsyncForTesting(WebApkInstaller* installer,
                                     const FinishCallback& finish_callback);

  // Calls the private function |UpdateAsync| for testing.
  // Should be used only for testing.
  static void UpdateAsyncForTesting(
      WebApkInstaller* installer,
      const std::string& webapk_package,
      int webapk_version,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale,
      const FinishCallback& callback);

  // Sets the timeout for the server requests.
  void SetTimeoutMs(int timeout_ms);

  // Called once the installation is complete or failed.
  void OnInstallFinished(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint result);

  // Asynchronously builds the WebAPK proto on a background thread for an update
  // or install request. Runs |callback| on the calling thread when complete.
  static void BuildProto(
      const ShortcutInfo& shortcut_info,
      const SkBitmap& primary_icon,
      const SkBitmap& badge_icon,
      const std::string& package_name,
      const std::string& version,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale,
      const base::Callback<void(std::unique_ptr<webapk::WebApk>)>& callback);

  // Registers JNI hooks.
  static bool Register(JNIEnv* env);

 protected:
  WebApkInstaller(content::BrowserContext* browser_context,
                  const ShortcutInfo& shortcut_info,
                  const SkBitmap& primary_icon,
                  const SkBitmap& badge_icon);

  // Called when the package name of the WebAPK is available and the install
  // or update request should be issued.
  virtual void InstallOrUpdateWebApk(const std::string& package_name,
                                     int version,
                                     const std::string& token);

  // Called when the install or update process has completed or failed.
  void OnResult(WebApkInstallResult result);

 private:
  enum TaskType {
    UNDEFINED,
    INSTALL,
    UPDATE,
  };

  // Create the Java object.
  void CreateJavaRef();

  // Talks to the Chrome WebAPK server to generate a WebAPK on the server and to
  // Google Play to install the downloaded WebAPK. Calls |callback| once the
  // install completed or failed.
  void InstallAsync(const FinishCallback& finish_callback);

  // Talks to the Chrome WebAPK server to update a WebAPK on the server and to
  // the Google Play server to install the downloaded WebAPK. Calls |callback|
  // after the request to install the WebAPK is sent to the Google Play server.
  void UpdateAsync(
      const std::string& webapk_package,
      int webapk_version,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale,
      const FinishCallback& callback);

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Called with the computed Murmur2 hash for the primary icon.
  void OnGotPrimaryIconMurmur2Hash(const std::string& primary_icon_hash);

  // Called with the computed Murmur2 hash for the badge icon, and
  // |did_fetch_badge_icon| to indicate whether there was an attempt to fetch
  // badge icon.
  void OnGotBadgeIconMurmur2Hash(bool did_fetch_badge_icon,
                                 const std::string& primary_icon_hash,
                                 const std::string& badge_icon_hash);

  // Sends a request to WebAPK server to create/update WebAPK. During a
  // successful request the WebAPK server responds with the URL of the generated
  // WebAPK.
  void SendRequest(std::unique_ptr<webapk::WebApk> request_proto);

  net::URLRequestContextGetter* request_context_getter_;

  // Sends HTTP request to WebAPK server.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // Fails WebApkInstaller if WebAPK server takes too long to respond or if the
  // download takes too long.
  base::OneShotTimer timer_;

  // Tracks how long it takes to install a WebAPK.
  std::unique_ptr<base::ElapsedTimer> install_duration_timer_;

  // Callback to call once WebApkInstaller succeeds or fails.
  FinishCallback finish_callback_;

  // Web Manifest info.
  const ShortcutInfo shortcut_info_;

  // WebAPK primary icon.
  const SkBitmap primary_icon_;

  // WebAPK badge icon.
  const SkBitmap badge_icon_;

  // WebAPK server URL.
  GURL server_url_;

  // The number of milliseconds to wait for the WebAPK server to respond.
  int webapk_server_timeout_ms_;

  // WebAPK package name.
  std::string webapk_package_;

  // Whether the server wants the WebAPK to request updates less frequently.
  bool relax_updates_;

  // Indicates whether the installer is for installing or updating a WebAPK.
  TaskType task_type_;

  // Points to the Java Object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Used to get |weak_ptr_|.
  base::WeakPtrFactory<WebApkInstaller> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebApkInstaller);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_
