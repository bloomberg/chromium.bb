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

std::string FakeProfile::GetProfileUserName() const {
  return name_;
}

Profile::ProfileType FakeProfile::GetProfileType() const {
  return REGULAR_PROFILE;
}

base::FilePath FakeProfile::GetPath() const {
  return path_;
}

scoped_ptr<content::ZoomLevelDelegate> FakeProfile::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}

bool FakeProfile::IsOffTheRecord() const {
  return false;
}

content::DownloadManagerDelegate* FakeProfile::GetDownloadManagerDelegate() {
  return nullptr;
}

net::URLRequestContextGetter* FakeProfile::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  return nullptr;
}

net::URLRequestContextGetter* FakeProfile::GetMediaRequestContext() {
  return nullptr;
}

net::URLRequestContextGetter*
FakeProfile::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  return nullptr;
}

net::URLRequestContextGetter*
FakeProfile::GetMediaRequestContextForStoragePartition(
        const base::FilePath& partition_path,
        bool in_memory) {
  return nullptr;
}

content::ResourceContext* FakeProfile::GetResourceContext() {
  return nullptr;
}

content::BrowserPluginGuestManager* FakeProfile::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* FakeProfile::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PushMessagingService* FakeProfile::GetPushMessagingService() {
  return nullptr;
}

content::SSLHostStateDelegate* FakeProfile::GetSSLHostStateDelegate() {
  return nullptr;
}

scoped_refptr<base::SequencedTaskRunner>
FakeProfile::GetIOTaskRunner() {
  return scoped_refptr<base::SequencedTaskRunner>();
}

Profile* FakeProfile::GetOffTheRecordProfile() {
  return nullptr;
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

bool FakeProfile::IsChild() {
  return false;
}

bool FakeProfile::IsLegacySupervised() {
  return false;
}

ExtensionSpecialStoragePolicy* FakeProfile::GetExtensionSpecialStoragePolicy() {
  return nullptr;
}

PrefService* FakeProfile::GetPrefs() {
  return nullptr;
}

const PrefService* FakeProfile::GetPrefs() const {
  return nullptr;
}

PrefService* FakeProfile::GetOffTheRecordPrefs() {
  return nullptr;
}

net::URLRequestContextGetter* FakeProfile::GetRequestContext() {
  return nullptr;
}

net::URLRequestContextGetter* FakeProfile::GetRequestContextForExtensions() {
  return nullptr;
}

net::SSLConfigService* FakeProfile::GetSSLConfigService() {
  return nullptr;
}

HostContentSettingsMap* FakeProfile::GetHostContentSettingsMap() {
  return nullptr;
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
  return nullptr;
}

net::URLRequestContextGetter*
FakeProfile::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return nullptr;
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
  return nullptr;
}

chrome_browser_net::Predictor* FakeProfile::GetNetworkPredictor() {
  return nullptr;
}

DevToolsNetworkController* FakeProfile::GetDevToolsNetworkController() {
  return nullptr;
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
