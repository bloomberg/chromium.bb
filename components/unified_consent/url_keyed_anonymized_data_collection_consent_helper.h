// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_URL_KEYED_ANONYMIZED_DATA_COLLECTION_CONSENT_HELPER_H_
#define COMPONENTS_UNIFIED_CONSENT_URL_KEYED_ANONYMIZED_DATA_COLLECTION_CONSENT_HELPER_H_

#include <memory>

#include "base/observer_list.h"

class PrefService;
namespace syncer {
class SyncService;
}

namespace unified_consent {

// Helper class that allows clients to check whether the user has consented
// for URL-keyed anonymized data collection.
class UrlKeyedAnonymizedDataCollectionConsentHelper {
 public:
  class Observer {
   public:
    // Called when the state of the URL-keyed anonymized data collection
    // changes.
    virtual void OnUrlKeyedDataCollectionConsentStateChanged(bool enabled) = 0;
  };

  // Creates a new |UrlKeyedAnonymizedDataCollectionConsentHelper| instance. We
  // distinguish the following cases:
  // 1. If |is_unified_consent_enabled| true, then the instance is backed by
  //    |pref_service|. Url-keyed data collection is enabled if the preference
  //    |prefs::kUrlKeyedAnonymizedDataCollectionEnabled| is set to true.
  //
  // 2. If |is_unified_consent_enabled| is false, then the instance is backed by
  //    the sync service. Url-keyed data collection is enabled if sync is active
  //    and if sync history is enabled.
  //
  // Note: |pref_service| must outlive the retuned instance.
  static std::unique_ptr<UrlKeyedAnonymizedDataCollectionConsentHelper>
  NewInstance(bool is_unified_consent_enabled,
              PrefService* pref_service,
              syncer::SyncService* sync_service);

  virtual ~UrlKeyedAnonymizedDataCollectionConsentHelper();

  // Returns true if the user has consented for URL keyed anonymized data
  // collection.
  virtual bool IsEnabled() = 0;

  // Methods to register or remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  UrlKeyedAnonymizedDataCollectionConsentHelper();

  // Fires |OnUrlKeyedDataCollectionConsentStateChanged| on all the observers.
  void FireOnStateChanged();

 private:
  base::ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(UrlKeyedAnonymizedDataCollectionConsentHelper);
};

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_URL_KEYED_ANONYMIZED_DATA_COLLECTION_CONSENT_HELPER_H_
