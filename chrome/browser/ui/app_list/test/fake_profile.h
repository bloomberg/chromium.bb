// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "components/domain_reliability/clear_mode.h"
#include "content/public/browser/browser_context.h"

class ResourceContext;

namespace net {
class URLRequestContextGetter;
}

namespace content {
class DownloadManagerDelegate;
class ResourceContext;
class SSLHostStateDelegate;
}

class FakeProfile : public Profile {
 public:
  explicit FakeProfile(const std::string& name);
  FakeProfile(const std::string& name, const base::FilePath& path);

  // Profile overrides.
  virtual std::string GetProfileName() override;
  virtual ProfileType GetProfileType() const override;
  virtual base::FilePath GetPath() const override;
  virtual bool IsOffTheRecord() const override;
  virtual content::DownloadManagerDelegate*
      GetDownloadManagerDelegate() override;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) override;
  virtual net::URLRequestContextGetter* GetMediaRequestContext() override;
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) override;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path,
          bool in_memory) override;
  virtual content::ResourceContext* GetResourceContext() override;
  virtual content::BrowserPluginGuestManager* GetGuestManager() override;
  virtual storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  virtual content::PushMessagingService* GetPushMessagingService() override;
  virtual content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  virtual scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  virtual Profile* GetOffTheRecordProfile() override;
  virtual void DestroyOffTheRecordProfile() override;
  virtual bool HasOffTheRecordProfile() override;
  virtual Profile* GetOriginalProfile() override;
  virtual bool IsSupervised() override;
  virtual history::TopSites* GetTopSites() override;
  virtual history::TopSites* GetTopSitesWithoutCreating() override;
  virtual ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() override;
  virtual PrefService* GetPrefs() override;
  virtual PrefService* GetOffTheRecordPrefs() override;
  virtual net::URLRequestContextGetter* GetRequestContext() override;
  virtual net::URLRequestContextGetter*
      GetRequestContextForExtensions() override;
  virtual net::SSLConfigService* GetSSLConfigService() override;
  virtual HostContentSettingsMap* GetHostContentSettingsMap() override;
  virtual bool IsSameProfile(Profile* profile) override;
  virtual base::Time GetStartTime() const override;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  virtual base::FilePath last_selected_directory() override;
  virtual void set_last_selected_directory(const base::FilePath& path) override;

#if defined(OS_CHROMEOS)
  virtual void ChangeAppLocale(
      const std::string& locale, AppLocaleChangedVia via) override;
  virtual void OnLogin() override;
  virtual void InitChromeOSPreferences() override;
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker() override;
  virtual chrome_browser_net::Predictor* GetNetworkPredictor() override;
  virtual DevToolsNetworkController* GetDevToolsNetworkController() override;
  virtual void ClearNetworkingHistorySince(
      base::Time time, const base::Closure& completion) override;
  virtual GURL GetHomePage() override;
  virtual bool WasCreatedByVersionOrLater(const std::string& version) override;
  virtual void SetExitType(ExitType exit_type) override;
  virtual ExitType GetLastSessionExitType() override;

 private:
  std::string name_;
  base::FilePath path_;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_H_
