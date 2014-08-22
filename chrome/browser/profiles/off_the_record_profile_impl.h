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
  virtual std::string GetProfileName() OVERRIDE;
  virtual ProfileType GetProfileType() const OVERRIDE;
  virtual Profile* GetOffTheRecordProfile() OVERRIDE;
  virtual void DestroyOffTheRecordProfile() OVERRIDE;
  virtual bool HasOffTheRecordProfile() OVERRIDE;
  virtual Profile* GetOriginalProfile() OVERRIDE;
  virtual bool IsSupervised() OVERRIDE;
  virtual ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual PrefService* GetOffTheRecordPrefs() OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetRequestContextForExtensions() OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) OVERRIDE;
  virtual net::SSLConfigService* GetSSLConfigService() OVERRIDE;
  virtual HostContentSettingsMap* GetHostContentSettingsMap() OVERRIDE;
  virtual bool IsSameProfile(Profile* profile) OVERRIDE;
  virtual Time GetStartTime() const OVERRIDE;
  virtual history::TopSites* GetTopSitesWithoutCreating() OVERRIDE;
  virtual history::TopSites* GetTopSites() OVERRIDE;
  virtual base::FilePath last_selected_directory() OVERRIDE;
  virtual void set_last_selected_directory(const base::FilePath& path) OVERRIDE;
  virtual bool WasCreatedByVersionOrLater(const std::string& version) OVERRIDE;
  virtual void SetExitType(ExitType exit_type) OVERRIDE;
  virtual ExitType GetLastSessionExitType() OVERRIDE;

#if defined(OS_CHROMEOS)
  virtual void ChangeAppLocale(const std::string& locale,
                               AppLocaleChangedVia) OVERRIDE;
  virtual void OnLogin() OVERRIDE;
  virtual void InitChromeOSPreferences() OVERRIDE;
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker() OVERRIDE;

  virtual chrome_browser_net::Predictor* GetNetworkPredictor() OVERRIDE;
  virtual DevToolsNetworkController* GetDevToolsNetworkController() OVERRIDE;
  virtual void ClearNetworkingHistorySince(
      base::Time time,
      const base::Closure& completion) OVERRIDE;
  virtual GURL GetHomePage() OVERRIDE;

  // content::BrowserContext implementation:
  virtual base::FilePath GetPath() const OVERRIDE;
  virtual scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() OVERRIDE;
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual content::DownloadManagerDelegate*
      GetDownloadManagerDelegate() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
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
