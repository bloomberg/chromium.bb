// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IMPL_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/browser/profiles/off_the_record_profile_io_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/domain_reliability/clear_mode.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/host_zoom_map.h"

using base::Time;
using base::TimeDelta;

class PrefServiceSyncable;

////////////////////////////////////////////////////////////////////////////////
//
// OffTheRecordProfileImpl is a profile subclass that wraps an existing profile
// to make it suitable for the incognito mode.
//
// Note: This class is a leaf class and is not intended for subclassing.
// Providing this header file is for unit testing.
//
////////////////////////////////////////////////////////////////////////////////
class OffTheRecordProfileImpl : public Profile {
 public:
  explicit OffTheRecordProfileImpl(Profile* real_profile);
  virtual ~OffTheRecordProfileImpl();
  void Init();

  // Profile implementation.
  virtual std::string GetProfileName() override;
  virtual ProfileType GetProfileType() const override;
  virtual Profile* GetOffTheRecordProfile() override;
  virtual void DestroyOffTheRecordProfile() override;
  virtual bool HasOffTheRecordProfile() override;
  virtual Profile* GetOriginalProfile() override;
  virtual bool IsSupervised() override;
  virtual ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() override;
  virtual PrefService* GetPrefs() override;
  virtual PrefService* GetOffTheRecordPrefs() override;
  virtual net::URLRequestContextGetter*
      GetRequestContextForExtensions() override;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  virtual net::SSLConfigService* GetSSLConfigService() override;
  virtual HostContentSettingsMap* GetHostContentSettingsMap() override;
  virtual bool IsSameProfile(Profile* profile) override;
  virtual Time GetStartTime() const override;
  virtual history::TopSites* GetTopSitesWithoutCreating() override;
  virtual history::TopSites* GetTopSites() override;
  virtual base::FilePath last_selected_directory() override;
  virtual void set_last_selected_directory(const base::FilePath& path) override;
  virtual bool WasCreatedByVersionOrLater(const std::string& version) override;
  virtual void SetExitType(ExitType exit_type) override;
  virtual ExitType GetLastSessionExitType() override;

#if defined(OS_CHROMEOS)
  virtual void ChangeAppLocale(const std::string& locale,
                               AppLocaleChangedVia) override;
  virtual void OnLogin() override;
  virtual void InitChromeOSPreferences() override;
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker() override;

  virtual chrome_browser_net::Predictor* GetNetworkPredictor() override;
  virtual DevToolsNetworkController* GetDevToolsNetworkController() override;
  virtual void ClearNetworkingHistorySince(
      base::Time time,
      const base::Closure& completion) override;
  virtual GURL GetHomePage() override;

  // content::BrowserContext implementation:
  virtual base::FilePath GetPath() const override;
  virtual scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  virtual bool IsOffTheRecord() const override;
  virtual content::DownloadManagerDelegate*
      GetDownloadManagerDelegate() override;
  virtual net::URLRequestContextGetter* GetRequestContext() override;
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

 private:
  FRIEND_TEST_ALL_PREFIXES(OffTheRecordProfileImplTest, GetHostZoomMap);
  void InitIoData();
  void InitHostZoomMap();

#if defined(OS_ANDROID) || defined(OS_IOS)
  void UseSystemProxy();
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change);
  PrefProxyConfigTracker* CreateProxyConfigTracker();

  // The real underlying profile.
  Profile* profile_;

  // Weak pointer owned by |profile_|.
  PrefServiceSyncable* prefs_;

  scoped_ptr<OffTheRecordProfileIOData::Handle> io_data_;

  // We use a non-persistent content settings map for OTR.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  // Time we were started.
  Time start_time_;

  base::FilePath last_selected_directory_;

  scoped_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  scoped_ptr<content::HostZoomMap::Subscription> zoom_subscription_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordProfileImpl);
};

#endif  // CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IMPL_H_
