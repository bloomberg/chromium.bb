// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_METRICS_SERVICE_H_
#define CHROME_BROWSER_PREFS_PREF_METRICS_SERVICE_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/synced_pref_change_registrar.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class PrefRegistrySimple;

// PrefMetricsService is responsible for recording prefs-related UMA stats.
class PrefMetricsService : public BrowserContextKeyedService {
 public:
  enum HashedPrefStyle {
    HASHED_PREF_STYLE_NEW,
    HASHED_PREF_STYLE_DEPRECATED,
  };

  explicit PrefMetricsService(Profile* profile);
  virtual ~PrefMetricsService();

  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static Factory* GetInstance();
    static PrefMetricsService* GetForProfile(Profile* profile);
   private:
    friend struct DefaultSingletonTraits<Factory>;

    Factory();
    virtual ~Factory();

    // BrowserContextKeyedServiceFactory implementation
    virtual BrowserContextKeyedService* BuildServiceInstanceFor(
        content::BrowserContext* profile) const OVERRIDE;
    virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
    virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
    virtual content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const OVERRIDE;
  };

  // Registers preferences in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  friend class PrefMetricsServiceTest;

  // Function to log a Value to a histogram
  typedef base::Callback<void(const std::string&, const Value*)>
      LogHistogramValueCallback;

  // For unit testing only.
  PrefMetricsService(Profile* profile,
                     PrefService* local_settings,
                     const std::string& device_id,
                     const char** tracked_pref_paths,
                     int tracked_pref_path_count);

  // Record prefs state on browser context creation.
  void RecordLaunchPrefs();

  // Register callbacks for synced pref changes.
  void RegisterSyncedPrefObservers();

  // Registers a histogram logging callback for a synced pref change.
  void AddPrefObserver(const std::string& path,
                       const std::string& histogram_name_prefix,
                       const LogHistogramValueCallback& callback);

  // Generic callback to observe a synced pref change.
  void OnPrefChanged(const std::string& histogram_name_prefix,
                     const LogHistogramValueCallback& callback,
                     const std::string& path,
                     bool from_sync);

  // Callback for a boolean pref change histogram.
  void LogBooleanPrefChange(const std::string& histogram_name,
                            const Value* value);

  // Callback for an integer pref change histogram.
  void LogIntegerPrefChange(int boundary_value,
                            const std::string& histogram_name,
                            const Value* value);

  // Callback for a list pref change. Each item in the list
  // is logged using the given histogram callback.
  void LogListPrefChange(const LogHistogramValueCallback& item_callback,
                         const std::string& histogram_name,
                         const Value* value);

  // Callback to receive a unique device_id.
  void GetDeviceIdCallback(const std::string& device_id);

  // Checks the tracked preferences against their last known values and reports
  // any discrepancies. This must be called after |device_id| has been set.
  void CheckTrackedPreferences();

  // Updates the hash of the tracked preference in local state. This must be
  // called after |device_id| has been set.
  void UpdateTrackedPreference(const char* path);

  // Removes the tracked preference from local state. Returns 'true' iff. the
  // value was present.
  bool RemoveTrackedPreference(const char* path);

  // Computes an MD5 hash for the given preference value.
  std::string GetHashedPrefValue(
      const char* path,
      const base::Value* value,
      HashedPrefStyle desired_style);

  void InitializePrefObservers();

  Profile* profile_;
  PrefService* prefs_;
  PrefService* local_state_;
  std::string profile_name_;
  std::string pref_hash_seed_;
  std::string device_id_;
  const char** tracked_pref_paths_;
  const int tracked_pref_path_count_;
  bool checked_tracked_prefs_;

  PrefChangeRegistrar pref_registrar_;
  scoped_ptr<SyncedPrefChangeRegistrar> synced_pref_change_registrar_;

  base::WeakPtrFactory<PrefMetricsService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrefMetricsService);
};

#endif  // CHROME_BROWSER_PREFS_PREF_METRICS_SERVICE_H_
