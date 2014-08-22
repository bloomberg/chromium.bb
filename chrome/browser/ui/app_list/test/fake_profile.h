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
  virtual std::string GetProfileName() OVERRIDE;
  virtual ProfileType GetProfileType() const OVERRIDE;
  virtual base::FilePath GetPath() const OVERRIDE;
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual content::DownloadManagerDelegate*
      GetDownloadManagerDelegate() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path,
          bool in_memory) OVERRIDE;
  virtual content::ResourceContext* GetResourceContext() OVERRIDE;
  virtual content::BrowserPluginGuestManager* GetGuestManager() OVERRIDE;
  virtual storage::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;
  virtual content::PushMessagingService* GetPushMessagingService() OVERRIDE;
  virtual content::SSLHostStateDelegate* GetSSLHostStateDelegate() OVERRIDE;
  virtual scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() OVERRIDE;
  virtual Profile* GetOffTheRecordProfile() OVERRIDE;
  virtual void DestroyOffTheRecordProfile() OVERRIDE;
  virtual bool HasOffTheRecordProfile() OVERRIDE;
  virtual Profile* GetOriginalProfile() OVERRIDE;
  virtual bool IsSupervised() OVERRIDE;
  virtual history::TopSites* GetTopSites() OVERRIDE;
  virtual history::TopSites* GetTopSitesWithoutCreating() OVERRIDE;
  virtual ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual PrefService* GetOffTheRecordPrefs() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetRequestContextForExtensions() OVERRIDE;
  virtual net::SSLConfigService* GetSSLConfigService() OVERRIDE;
  virtual HostContentSettingsMap* GetHostContentSettingsMap() OVERRIDE;
  virtual bool IsSameProfile(Profile* profile) OVERRIDE;
  virtual base::Time GetStartTime() const OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) OVERRIDE;
  virtual base::FilePath last_selected_directory() OVERRIDE;
  virtual void set_last_selected_directory(const base::FilePath& path) OVERRIDE;

#if defined(OS_CHROMEOS)
  virtual void ChangeAppLocale(
      const std::string& locale, AppLocaleChangedVia via) OVERRIDE;
  virtual void OnLogin() OVERRIDE;
  virtual void InitChromeOSPreferences() OVERRIDE;
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker() OVERRIDE;
  virtual chrome_browser_net::Predictor* GetNetworkPredictor() OVERRIDE;
  virtual DevToolsNetworkController* GetDevToolsNetworkController() OVERRIDE;
  virtual void ClearNetworkingHistorySince(
      base::Time time, const base::Closure& completion) OVERRIDE;
  virtual GURL GetHomePage() OVERRIDE;
  virtual bool WasCreatedByVersionOrLater(const std::string& version) OVERRIDE;
  virtual void SetExitType(ExitType exit_type) OVERRIDE;
  virtual ExitType GetLastSessionExitType() OVERRIDE;

 private:
  std::string name_;
  base::FilePath path_;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_H_
