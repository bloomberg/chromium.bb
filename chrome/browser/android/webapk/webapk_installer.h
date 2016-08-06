// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_

#include <jni.h>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/net/file_downloader.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace webapk {
class CreateWebApkRequest;
}

// Talks to Chrome WebAPK server and Google Play to generate a WebAPK on the
// server, download it, and install it.
class WebApkInstaller : public net::URLFetcherDelegate {
 public:
  using FinishCallback = base::Callback<void(bool)>;

  WebApkInstaller(const ShortcutInfo& shortcut_info,
                  const SkBitmap& shorcut_icon);
  ~WebApkInstaller() override;

  // Register JNI methods.
  static bool Register(JNIEnv* env);

  // Talks to the Chrome WebAPK server to generate a WebAPK on the server and to
  // Google Play to install the generated WebAPK. Calls |callback| after the
  // request to install the WebAPK is sent to Google Play.
  void InstallAsync(content::BrowserContext* browser_context,
                    const FinishCallback& callback);

  // Same as InstallAsync() but uses the passed in |request_context_getter|.
  void InstallAsyncWithURLRequestContextGetter(
      net::URLRequestContextGetter* request_context_getter,
      const FinishCallback& callback);

 protected:
  // Starts installation of the downloaded WebAPK. Returns whether the install
  // could be started. The installation may still fail if true is returned.
  // |file_path| is the file path that the WebAPK was downloaded to.
  // |package_name| is the package name that the WebAPK should be installed at.
  virtual bool StartDownloadedWebApkInstall(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jstring>& java_file_path,
      const base::android::ScopedJavaLocalRef<jstring>& java_package_name);

 private:
  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Initializes |request_context_getter_| on UI thread.
  void InitializeRequestContextGetterOnUIThread(
      content::BrowserContext* browser_context);

  // Sends request to WebAPK server to create WebAPK. During a successful
  // request the WebAPK server responds with the URL of the generated WebAPK.
  void SendCreateWebApkRequest();

  // Called with the URL of generated WebAPK and the package name that the
  // WebAPK should be installed at.
  void OnGotWebApkDownloadUrl(const std::string& download_url,
                              const std::string& package_name);

  // Called once the WebAPK has been downloaded. Installs the WebAPK if the
  // download was successful.
  // |file_path| is the file path that the WebAPK was downloaded to.
  // |package_name| is the package name that the WebAPK should be installed at.
  void OnWebApkDownloaded(const base::FilePath& file_path,
                          const std::string& package_name,
                          FileDownloader::Result result);

  // Populates webapk::CreateWebApkRequest and returns it.
  std::unique_ptr<webapk::CreateWebApkRequest> BuildCreateWebApkRequest();

  // Called when the request to the WebAPK server times out or when the WebAPK
  // download times out.
  void OnTimeout();

  // Called when the request to install the WebAPK is sent to Google Play.
  void OnSuccess();

  // Called if a WebAPK could not be created. WebApkInstaller only tracks the
  // WebAPK creation and the WebAPK download. It does not track the
  // WebAPK installation. OnFailure() is not called if the WebAPK could not be
  // installed.
  void OnFailure();

  net::URLRequestContextGetter* request_context_getter_;

  // Sends HTTP request to WebAPK server.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // Downloads WebAPK.
  std::unique_ptr<FileDownloader> downloader_;

  // Fails WebApkInstaller if WebAPK server takes too long to respond or if the
  // download takes too long.
  base::OneShotTimer timer_;

  // Callback to call once WebApkInstaller succeeds or fails.
  FinishCallback finish_callback_;

  // Web Manifest info.
  const ShortcutInfo shortcut_info_;

  // WebAPK app icon.
  const SkBitmap shortcut_icon_;

  // WebAPK server URL.
  GURL server_url_;

  // Used to get |weak_ptr_| on the IO thread.
  base::WeakPtrFactory<WebApkInstaller> io_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebApkInstaller);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_
