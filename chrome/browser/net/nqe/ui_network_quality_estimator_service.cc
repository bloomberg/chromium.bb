// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "net/nqe/network_qualities_prefs_manager.h"

namespace {

// PrefDelegateImpl writes the provided dictionary value to the network quality
// estimator prefs on the disk.
class PrefDelegateImpl
    : public net::NetworkQualitiesPrefsManager::PrefDelegate {
 public:
  // |pref_service| is used to read and write prefs from/to the disk.
  explicit PrefDelegateImpl(PrefService* pref_service)
      : pref_service_(pref_service), path_(prefs::kNetworkQualities) {
    DCHECK(pref_service_);
  }
  ~PrefDelegateImpl() override {}

  void SetDictionaryValue(const base::DictionaryValue& value) override {
    DCHECK(thread_checker_.CalledOnValidThread());

    pref_service_->Set(path_, value);
    UMA_HISTOGRAM_EXACT_LINEAR("NQE.Prefs.WriteCount", 1, 2);
  }

  std::unique_ptr<base::DictionaryValue> GetDictionaryValue() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    UMA_HISTOGRAM_EXACT_LINEAR("NQE.Prefs.ReadCount", 1, 2);
    return pref_service_->GetDictionary(path_)->CreateDeepCopy();
  }

 private:
  PrefService* pref_service_;

  // |path_| is the location of the network quality estimator prefs.
  const std::string path_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PrefDelegateImpl);
};

// Initializes |pref_manager| on |io_thread|.
void SetNQEOnIOThread(net::NetworkQualitiesPrefsManager* prefs_manager,
                      IOThread* io_thread) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Avoid null pointer referencing during browser shutdown.
  if (!io_thread->globals()->network_quality_estimator)
    return;

  prefs_manager->InitializeOnNetworkThread(
      io_thread->globals()->network_quality_estimator.get());
}

}  // namespace

// A class that sets itself as an observer of the EffectiveconnectionType for
// the browser IO thread. It reports any change in EffectiveConnectionType back
// to the UI service.
// It is created on the UI thread, but used and deleted on the IO thread.
class UINetworkQualityEstimatorService::IONetworkQualityObserver
    : public net::NetworkQualityEstimator::EffectiveConnectionTypeObserver {
 public:
  explicit IONetworkQualityObserver(
      base::WeakPtr<UINetworkQualityEstimatorService> service)
      : service_(service), network_quality_estimator_(nullptr) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ~IONetworkQualityObserver() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (network_quality_estimator_)
      network_quality_estimator_->RemoveEffectiveConnectionTypeObserver(this);
  }

  void InitializeOnIOThread(IOThread* io_thread) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (!io_thread->globals()->network_quality_estimator)
      return;
    network_quality_estimator_ =
        io_thread->globals()->network_quality_estimator.get();
    if (!network_quality_estimator_)
      return;
    network_quality_estimator_->AddEffectiveConnectionTypeObserver(this);
  }

  // net::NetworkQualityEstimator::EffectiveConnectionTypeObserver
  // implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(
            &UINetworkQualityEstimatorService::EffectiveConnectionTypeChanged,
            service_, type));
  }

 private:
  base::WeakPtr<UINetworkQualityEstimatorService> service_;
  net::NetworkQualityEstimator* network_quality_estimator_;

  DISALLOW_COPY_AND_ASSIGN(IONetworkQualityObserver);
};

UINetworkQualityEstimatorService::UINetworkQualityEstimatorService(
    Profile* profile)
    : type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN), weak_factory_(this) {
  DCHECK(profile);
  // If this is running in a context without an IOThread, don't try to create
  // the IO object.
  if (!g_browser_process->io_thread())
    return;
  io_observer_ = base::WrapUnique(
      new IONetworkQualityObserver(weak_factory_.GetWeakPtr()));
  std::unique_ptr<PrefDelegateImpl> pref_delegate(
      new PrefDelegateImpl(profile->GetPrefs()));
  prefs_manager_ = base::WrapUnique(
      new net::NetworkQualitiesPrefsManager(std::move(pref_delegate)));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&IONetworkQualityObserver::InitializeOnIOThread,
                 base::Unretained(io_observer_.get()),
                 g_browser_process->io_thread()));
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&SetNQEOnIOThread, prefs_manager_.get(),
                 g_browser_process->io_thread()));
}

UINetworkQualityEstimatorService::~UINetworkQualityEstimatorService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void UINetworkQualityEstimatorService::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  weak_factory_.InvalidateWeakPtrs();
  if (io_observer_) {
    bool deleted = content::BrowserThread::DeleteSoon(
        content::BrowserThread::IO, FROM_HERE, io_observer_.release());
    DCHECK(deleted);
    // Silence unused variable warning in release builds.
    (void)deleted;
  }
  if (prefs_manager_) {
    prefs_manager_->ShutdownOnPrefThread();
    bool deleted = content::BrowserThread::DeleteSoon(
        content::BrowserThread::IO, FROM_HERE, prefs_manager_.release());
    DCHECK(deleted);
    // Silence unused variable warning in release builds.
    (void)deleted;
  }
}

void UINetworkQualityEstimatorService::EffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  type_ = type;
  for (auto& observer : effective_connection_type_observer_list_)
    observer.OnEffectiveConnectionTypeChanged(type);
}

void UINetworkQualityEstimatorService::AddEffectiveConnectionTypeObserver(
    net::NetworkQualityEstimator::EffectiveConnectionTypeObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  effective_connection_type_observer_list_.AddObserver(observer);

  // Notify the |observer| on the next message pump since |observer| may not
  // be completely set up for receiving the callbacks.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&UINetworkQualityEstimatorService::
                     NotifyEffectiveConnectionTypeObserverIfPresent,
                 weak_factory_.GetWeakPtr(), observer));
}

void UINetworkQualityEstimatorService::RemoveEffectiveConnectionTypeObserver(
    net::NetworkQualityEstimator::EffectiveConnectionTypeObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  effective_connection_type_observer_list_.RemoveObserver(observer);
}

void UINetworkQualityEstimatorService::SetEffectiveConnectionTypeForTesting(
    net::EffectiveConnectionType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  EffectiveConnectionTypeChanged(type);
}

net::EffectiveConnectionType
UINetworkQualityEstimatorService::GetEffectiveConnectionType() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return type_;
}

void UINetworkQualityEstimatorService::ClearPrefs() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!prefs_manager_)
    return;
  prefs_manager_->ClearPrefs();
}

void UINetworkQualityEstimatorService::
    NotifyEffectiveConnectionTypeObserverIfPresent(
        net::NetworkQualityEstimator::EffectiveConnectionTypeObserver* observer)
        const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!effective_connection_type_observer_list_.HasObserver(observer))
    return;
  if (type_ == net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN)
    return;
  observer->OnEffectiveConnectionTypeChanged(type_);
}

// static
void UINetworkQualityEstimatorService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNetworkQualities,
                                   PrefRegistry::LOSSY_PREF);
}

std::map<net::nqe::internal::NetworkID,
         net::nqe::internal::CachedNetworkQuality>
UINetworkQualityEstimatorService::ForceReadPrefsForTesting() const {
  if (!prefs_manager_) {
    return std::map<net::nqe::internal::NetworkID,
                    net::nqe::internal::CachedNetworkQuality>();
  }
  return prefs_manager_->ForceReadPrefsForTesting();
}
