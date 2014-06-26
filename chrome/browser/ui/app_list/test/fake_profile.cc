// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/fake_profile.h"

FakeProfile::FakeProfile(const std::string& name)
    : name_(name) {
}

FakeProfile::FakeProfile(const std::string& name, const base::FilePath& path)
    : name_(name),
      path_(path) {
}

std::string FakeProfile::GetProfileName() {
  return name_;
}

Profile::ProfileType FakeProfile::GetProfileType() const {
  return REGULAR_PROFILE;
}

base::FilePath FakeProfile::GetPath() const {
  return path_;
}

bool FakeProfile::IsOffTheRecord() const {
  return false;
}

content::DownloadManagerDelegate*
FakeProfile::GetDownloadManagerDelegate() {
  return NULL;
}

net::URLRequestContextGetter* FakeProfile::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  return NULL;
}

net::URLRequestContextGetter* FakeProfile::GetMediaRequestContext() {
  return NULL;
}

net::URLRequestContextGetter*
FakeProfile::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  return NULL;
}

net::URLRequestContextGetter*
FakeProfile::GetMediaRequestContextForStoragePartition(
        const base::FilePath& partition_path,
        bool in_memory) {
  return NULL;
}

content::ResourceContext* FakeProfile::GetResourceContext() {
  return NULL;
}

content::BrowserPluginGuestManager* FakeProfile::GetGuestManager() {
  return NULL;
}

quota::SpecialStoragePolicy* FakeProfile::GetSpecialStoragePolicy() {
  return NULL;
}

content::PushMessagingService* FakeProfile::GetPushMessagingService() {
  return NULL;
}

scoped_refptr<base::SequencedTaskRunner>
FakeProfile::GetIOTaskRunner() {
  return scoped_refptr<base::SequencedTaskRunner>();
}

Profile* FakeProfile::GetOffTheRecordProfile() {
  return NULL;
}

void FakeProfile::DestroyOffTheRecordProfile() {}

bool FakeProfile::HasOffTheRecordProfile() {
  return false;
}

Profile* FakeProfile::GetOriginalProfile() {
  return this;
}

bool FakeProfile::IsSupervised() {
  return false;
}

history::TopSites* FakeProfile::GetTopSites() {
  return NULL;
}

history::TopSites* FakeProfile::GetTopSitesWithoutCreating() {
  return NULL;
}

ExtensionSpecialStoragePolicy* FakeProfile::GetExtensionSpecialStoragePolicy() {
  return NULL;
}

PrefService* FakeProfile::GetPrefs() {
  return NULL;
}

PrefService* FakeProfile::GetOffTheRecordPrefs() {
  return NULL;
}

net::URLRequestContextGetter* FakeProfile::GetRequestContext() {
  return NULL;
}

net::URLRequestContextGetter* FakeProfile::GetRequestContextForExtensions() {
  return NULL;
}

net::SSLConfigService* FakeProfile::GetSSLConfigService() {
  return NULL;
}

HostContentSettingsMap* FakeProfile::GetHostContentSettingsMap() {
  return NULL;
}

bool FakeProfile::IsSameProfile(Profile* profile) {
  return false;
}

base::Time FakeProfile::GetStartTime() const {
  return base::Time();
}

net::URLRequestContextGetter* FakeProfile::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return NULL;
}

net::URLRequestContextGetter*
FakeProfile::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return NULL;
}

base::FilePath FakeProfile::last_selected_directory() {
  return base::FilePath();
}

void FakeProfile::set_last_selected_directory(const base::FilePath& path) {}

#if defined(OS_CHROMEOS)
void FakeProfile::ChangeAppLocale(
    const std::string& locale, AppLocaleChangedVia via) {}
void FakeProfile::OnLogin() {}
void FakeProfile::InitChromeOSPreferences() {}
#endif  // defined(OS_CHROMEOS)

PrefProxyConfigTracker* FakeProfile::GetProxyConfigTracker() {
  return NULL;
}

chrome_browser_net::Predictor* FakeProfile::GetNetworkPredictor() {
  return NULL;
}

DevToolsNetworkController* FakeProfile::GetDevToolsNetworkController() {
  return NULL;
}

void FakeProfile::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
}

GURL FakeProfile::GetHomePage() {
  return GURL();
}

bool FakeProfile::WasCreatedByVersionOrLater(const std::string& version) {
  return false;
}

void FakeProfile::SetExitType(ExitType exit_type) {
}

Profile::ExitType FakeProfile::GetLastSessionExitType() {
  return EXIT_NORMAL;
}
