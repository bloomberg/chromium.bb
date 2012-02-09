// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_installer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/plugin_download_helper.h"
#include "chrome/browser/plugin_installer_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

PluginInstaller::~PluginInstaller() {
}

PluginInstaller::PluginInstaller(const std::string& identifier,
                                 const GURL& plugin_url,
                                 const GURL& help_url,
                                 const string16& name,
                                 bool url_for_display)
    : state_(kStateIdle),
      identifier_(identifier),
      plugin_url_(plugin_url),
      help_url_(help_url),
      name_(name),
      url_for_display_(url_for_display) {
}

void PluginInstaller::AddObserver(PluginInstallerObserver* observer) {
  observers_.AddObserver(observer);
}

void PluginInstaller::RemoveObserver(PluginInstallerObserver* observer) {
  observers_.RemoveObserver(observer);
  if (observers_.size() == weak_observers_.size()) {
    FOR_EACH_OBSERVER(WeakPluginInstallerObserver, weak_observers_,
                      OnlyWeakObserversLeft());
  }
}

void PluginInstaller::AddWeakObserver(WeakPluginInstallerObserver* observer) {
  weak_observers_.AddObserver(observer);
}

void PluginInstaller::RemoveWeakObserver(
    WeakPluginInstallerObserver* observer) {
  weak_observers_.RemoveObserver(observer);
}

void PluginInstaller::StartInstalling(content::WebContents* web_contents) {
  DCHECK(state_ == kStateIdle);
  DCHECK(!url_for_display_);
  state_ = kStateDownloading;
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DidStartDownload());
  // |downloader| will delete itself after running the callback.
  PluginDownloadUrlHelper* downloader = new PluginDownloadUrlHelper();
  downloader->InitiateDownload(
      plugin_url_,
      web_contents->GetBrowserContext()->GetRequestContext(),
      base::Bind(&PluginInstaller::DidFinishDownload, base::Unretained(this)),
      base::Bind(&PluginInstaller::DownloadError, base::Unretained(this)));
}

void PluginInstaller::OpenDownloadURL(content::WebContents* web_contents) {
  DCHECK(state_ == kStateIdle);
  DCHECK(url_for_display_);
  web_contents->OpenURL(content::OpenURLParams(
      plugin_url_,
      content::Referrer(web_contents->GetURL(),
                        WebKit::WebReferrerPolicyDefault),
      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_TYPED, false));
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DidFinishDownload());
}

void PluginInstaller::DidFinishDownload(const FilePath& downloaded_file) {
  DCHECK(state_ == kStateDownloading);
  state_ = kStateIdle;
  DVLOG(1) << "Plug-in installer is at \"" << downloaded_file.value() << "\"";
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DidFinishDownload());
  platform_util::OpenItem(downloaded_file);
}

void PluginInstaller::DownloadError(const std::string& msg) {
  DCHECK(state_ == kStateDownloading);
  state_ = kStateIdle;
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DownloadError(msg));
}
