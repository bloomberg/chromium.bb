// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_prober.h"

#include <math.h>
#include <cmath>

#include "base/base64.h"
#include "base/bind.h"
#include "base/guid.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/previews/proto/previews_prober_cache_entry.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#include "net/base/network_interfaces.h"
#endif

namespace {

const char kCachePrefKeyPrefix[] = "previews.prober.cache";

const char kSuccessHistogram[] = "Previews.Prober.DidSucceed";
const char kAttemptsBeforeSuccessHistogram[] =
    "Previews.Prober.NumAttemptsBeforeSuccess";
const char kHttpRespCodeHistogram[] = "Previews.Prober.ResponseCode";
const char kNetErrorHistogram[] = "Previews.Prober.NetError";

// Please keep this up to date with logged histogram suffix
// |Previews.Prober.Clients| in tools/metrics/histograms/histograms.xml.
// These names are also used in prefs so they should not be changed without
// consideration for removing the old value.
std::string NameForClient(PreviewsProber::ClientName name) {
  switch (name) {
    case PreviewsProber::ClientName::kLitepages:
      return "Litepages";
  }
  NOTREACHED();
  return std::string();
}

std::string PrefKeyForName(const std::string& name) {
  return base::StringPrintf("%s.%s", kCachePrefKeyPrefix, name.c_str());
}

std::string HttpMethodToString(PreviewsProber::HttpMethod http_method) {
  switch (http_method) {
    case PreviewsProber::HttpMethod::kGet:
      return "GET";
    case PreviewsProber::HttpMethod::kHead:
      return "HEAD";
  }
}

// Computes the time delta for a given Backoff algorithm, a base interval, and
// the count of how many attempts have been made thus far.
base::TimeDelta ComputeNextTimeDeltaForBackoff(PreviewsProber::Backoff backoff,
                                               base::TimeDelta base_interval,
                                               size_t attempts_so_far) {
  switch (backoff) {
    case PreviewsProber::Backoff::kLinear:
      return base_interval;
    case PreviewsProber::Backoff::kExponential:
      return base_interval * pow(2, attempts_so_far);
  }
}

std::string GenerateNetworkID(
    network::NetworkConnectionTracker* network_connection_tracker) {
  network::mojom::ConnectionType connection_type =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  if (network_connection_tracker) {
    network_connection_tracker->GetConnectionType(&connection_type,
                                                  base::DoNothing());
  }

  std::string id = base::NumberToString(static_cast<int>(connection_type));
  bool is_cellular =
      network::NetworkConnectionTracker::IsConnectionCellular(connection_type);
  if (is_cellular) {
    // Don't care about cell connection type.
    id = "cell";
  }

// Further identify WiFi and cell connections. These calls are only supported
// for Android devices.
#if defined(OS_ANDROID)
  if (connection_type == network::mojom::ConnectionType::CONNECTION_WIFI) {
    return base::StringPrintf("%s,%s", id.c_str(), net::GetWifiSSID().c_str());
  }

  if (is_cellular) {
    return base::StringPrintf(
        "%s,%s", id.c_str(),
        net::android::GetTelephonyNetworkOperator().c_str());
  }
#endif

  return id;
}

base::Optional<base::Value> EncodeCacheEntryValue(
    const PreviewsProberCacheEntry& entry) {
  std::string serialized_entry;
  bool serialize_to_string_ok = entry.SerializeToString(&serialized_entry);
  if (!serialize_to_string_ok)
    return base::nullopt;

  std::string base64_encoded;
  base::Base64Encode(serialized_entry, &base64_encoded);
  return base::Value(base64_encoded);
}

base::Optional<PreviewsProberCacheEntry> DecodeCacheEntryValue(
    const base::Value& value) {
  if (!value.is_string())
    return base::nullopt;

  std::string base64_decoded;
  if (!base::Base64Decode(value.GetString(), &base64_decoded))
    return base::nullopt;

  PreviewsProberCacheEntry entry;
  if (!entry.ParseFromString(base64_decoded))
    return base::nullopt;

  return entry;
}

base::Time LastModifiedTimeFromCacheEntry(
    const PreviewsProberCacheEntry& entry) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(entry.last_modified()));
}

void RemoveOldestDictionaryEntry(base::DictionaryValue* dict) {
  std::vector<std::string> keys_to_remove;

  std::string oldest_key;
  base::Time oldest_mod_time = base::Time::Max();
  for (const auto& iter : dict->DictItems()) {
    base::Optional<PreviewsProberCacheEntry> entry =
        DecodeCacheEntryValue(iter.second);
    if (!entry.has_value()) {
      // Also remove anything that can't be decoded.
      keys_to_remove.push_back(iter.first);
      continue;
    }

    base::Time mod_time = LastModifiedTimeFromCacheEntry(entry.value());
    if (mod_time < oldest_mod_time) {
      oldest_key = iter.first;
      oldest_mod_time = mod_time;
    }
  }

  if (!oldest_key.empty()) {
    keys_to_remove.push_back(oldest_key);
  }

  for (const std::string& key : keys_to_remove) {
    dict->RemoveKey(key);
  }
}

#if defined(OS_ANDROID)
bool IsInForeground(base::android::ApplicationState state) {
  switch (state) {
    case base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
      return true;
    case base::android::APPLICATION_STATE_UNKNOWN:
    case base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
      return false;
  }
}
#endif

}  // namespace

PreviewsProber::RetryPolicy::RetryPolicy() = default;
PreviewsProber::RetryPolicy::~RetryPolicy() = default;
PreviewsProber::RetryPolicy::RetryPolicy(PreviewsProber::RetryPolicy const&) =
    default;
PreviewsProber::TimeoutPolicy::TimeoutPolicy() = default;
PreviewsProber::TimeoutPolicy::~TimeoutPolicy() = default;
PreviewsProber::TimeoutPolicy::TimeoutPolicy(
    PreviewsProber::TimeoutPolicy const&) = default;

PreviewsProber::PreviewsProber(
    Delegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    const ClientName name,
    const GURL& url,
    const HttpMethod http_method,
    const net::HttpRequestHeaders headers,
    const RetryPolicy& retry_policy,
    const TimeoutPolicy& timeout_policy,
    const size_t max_cache_entries,
    base::TimeDelta revalidate_cache_after)
    : PreviewsProber(delegate,
                     url_loader_factory,
                     pref_service,
                     name,
                     url,
                     http_method,
                     headers,
                     retry_policy,
                     timeout_policy,
                     max_cache_entries,
                     revalidate_cache_after,
                     base::DefaultTickClock::GetInstance(),
                     base::DefaultClock::GetInstance()) {}

PreviewsProber::PreviewsProber(
    Delegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    const ClientName name,
    const GURL& url,
    const HttpMethod http_method,
    const net::HttpRequestHeaders headers,
    const RetryPolicy& retry_policy,
    const TimeoutPolicy& timeout_policy,
    const size_t max_cache_entries,
    base::TimeDelta revalidate_cache_after,
    const base::TickClock* tick_clock,
    const base::Clock* clock)
    : delegate_(delegate),
      name_(NameForClient(name)),
      pref_key_(PrefKeyForName(NameForClient(name))),
      url_(url),
      http_method_(http_method),
      headers_(headers),
      retry_policy_(retry_policy),
      timeout_policy_(timeout_policy),
      max_cache_entries_(max_cache_entries),
      revalidate_cache_after_(revalidate_cache_after),
      successive_retry_count_(0),
      successive_timeout_count_(0),
      cached_probe_results_(std::make_unique<base::DictionaryValue>()),
      tick_clock_(tick_clock),
      clock_(clock),
      is_active_(false),
      network_connection_tracker_(nullptr),
      pref_service_(pref_service),
      url_loader_factory_(url_loader_factory),
      weak_factory_(this) {
  DCHECK(delegate_);
  DCHECK(pref_service_);

  // The NetworkConnectionTracker can only be used directly on the UI thread.
  // Otherwise we use the cross-thread call.
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) &&
      content::GetNetworkConnectionTracker()) {
    AddSelfAsNetworkConnectionObserver(content::GetNetworkConnectionTracker());
  } else {
    content::GetNetworkConnectionTrackerFromUIThread(
        base::BindOnce(&PreviewsProber::AddSelfAsNetworkConnectionObserver,
                       weak_factory_.GetWeakPtr()));
  }
  cached_probe_results_ =
      pref_service_->GetDictionary(pref_key_)->CreateDeepCopy();
}

PreviewsProber::~PreviewsProber() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (network_connection_tracker_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

// static
void PreviewsProber::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  for (int i = 0; i <= static_cast<int>(PreviewsProber::ClientName::kMaxValue);
       i++) {
    registry->RegisterDictionaryPref(PrefKeyForName(
        NameForClient(static_cast<PreviewsProber::ClientName>(i))));
  }
}

void PreviewsProber::AddSelfAsNetworkConnectionObserver(
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_connection_tracker_ = network_connection_tracker;
  network_connection_tracker_->AddNetworkConnectionObserver(this);
}

void PreviewsProber::ResetState() {
  is_active_ = false;
  successive_retry_count_ = 0;
  successive_timeout_count_ = 0;
  retry_timer_.reset();
  timeout_timer_.reset();
  url_loader_.reset();
#if defined(OS_ANDROID)
  application_status_listener_.reset();
#endif
}

void PreviewsProber::SendNowIfInactive(bool send_only_in_foreground) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_active_)
    return;

#if defined(OS_ANDROID)
  if (send_only_in_foreground &&
      !IsInForeground(base::android::ApplicationStatusListener::GetState())) {
    // base::Unretained is safe here because the callback is owned by
    // |application_status_listener_| which is owned by |this|.
    application_status_listener_ =
        base::android::ApplicationStatusListener::New(base::BindRepeating(
            &PreviewsProber::OnApplicationStateChange, base::Unretained(this)));
    return;
  }
#endif

  CreateAndStartURLLoader();
}

#if defined(OS_ANDROID)
void PreviewsProber::OnApplicationStateChange(
    base::android::ApplicationState new_state) {
  DCHECK(application_status_listener_);

  if (!IsInForeground(new_state))
    return;

  SendNowIfInactive(false);
  application_status_listener_.reset();
}
#endif

void PreviewsProber::OnConnectionChanged(network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If a probe is already in flight we don't want to continue to use it since
  // the network has just changed. Reset all state and start again.
  ResetState();
  CreateAndStartURLLoader();
}

void PreviewsProber::CreateAndStartURLLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_active_ || successive_retry_count_ > 0);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!url_loader_);

  if (!delegate_->ShouldSendNextProbe()) {
    ResetState();
    return;
  }

  is_active_ = true;

  GURL url = url_;
  if (retry_policy_.use_random_urls) {
    std::string query = "guid=" + base::GenerateGUID();
    GURL::Replacements replacements;
    replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
    url = url.ReplaceComponents(replacements);
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("previews_prober", R"(
        semantics {
          sender: "Previews Prober"
          description:
            "Requests a small resource to test network connectivity to a given "
            "resource or domain which will either be a Google owned domain or"
            "the website that the user is navigating to."
          trigger:
            "Requested when Lite mode and Previews are enabled on startup and "
            "on every network change."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control Lite mode on Android via the settings menu. "
            "Lite mode is not available on iOS, and on desktop only for "
            "developer testing."
          policy_exception_justification: "Not implemented."
        })");
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->method = HttpMethodToString(http_method_);
  request->headers = headers_;
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->allow_credentials = false;

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader_->SetAllowHttpErrorResults(true);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PreviewsProber::OnURLLoadComplete,
                     base::Unretained(this)),
      1024);

  // We don't use SimpleURLLoader's timeout functionality because it is not
  // possible to test by PreviewsProberTest.
  base::TimeDelta ttl = ComputeNextTimeDeltaForBackoff(
      timeout_policy_.backoff, timeout_policy_.base_timeout,
      successive_timeout_count_);
  timeout_timer_ = std::make_unique<base::OneShotTimer>(tick_clock_);
  // base::Unretained is safe because |timeout_timer_| is owned by this.
  timeout_timer_->Start(FROM_HERE, ttl,
                        base::BindOnce(&PreviewsProber::ProcessProbeTimeout,
                                       base::Unretained(this)));
}

void PreviewsProber::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();

    base::UmaHistogramSparse(AppendNameToHistogram(kHttpRespCodeHistogram),
                             std::abs(response_code));
  }

  base::UmaHistogramSparse(AppendNameToHistogram(kNetErrorHistogram),
                           std::abs(url_loader_->NetError()));

  bool was_successful = delegate_->IsResponseSuccess(
      static_cast<net::Error>(url_loader_->NetError()),
      *url_loader_->ResponseInfo(), std::move(response_body));

  timeout_timer_.reset();
  url_loader_.reset();
  successive_timeout_count_ = 0;

  if (was_successful) {
    ProcessProbeSuccess();
    return;
  }
  ProcessProbeFailure();
}

void PreviewsProber::ProcessProbeTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url_loader_);

  // Since we manually set the timeout handling of the probe, record the net
  // error here as well for simplicity.
  base::UmaHistogramSparse(AppendNameToHistogram(kNetErrorHistogram),
                           std::abs(net::ERR_TIMED_OUT));

  url_loader_.reset();
  successive_timeout_count_++;
  ProcessProbeFailure();
}

void PreviewsProber::ProcessProbeFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!timeout_timer_ || !timeout_timer_->IsRunning());
  DCHECK(!url_loader_);
  DCHECK(is_active_);

  RecordProbeResult(false);

  if (retry_policy_.max_retries > successive_retry_count_) {
    base::TimeDelta interval = ComputeNextTimeDeltaForBackoff(
        retry_policy_.backoff, retry_policy_.base_interval,
        successive_retry_count_);

    retry_timer_ = std::make_unique<base::OneShotTimer>(tick_clock_);
    // base::Unretained is safe because |retry_timer_| is owned by this.
    retry_timer_->Start(FROM_HERE, interval,
                        base::BindOnce(&PreviewsProber::CreateAndStartURLLoader,
                                       base::Unretained(this)));

    successive_retry_count_++;
    return;
  }

  ResetState();
}

void PreviewsProber::ProcessProbeSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!timeout_timer_ || !timeout_timer_->IsRunning());
  DCHECK(!url_loader_);
  DCHECK(is_active_);

  base::LinearHistogram::FactoryGet(
      AppendNameToHistogram(kAttemptsBeforeSuccessHistogram), 1 /* minimum */,
      25 /* maximum */, 25 /* bucket_count */,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      // |successive_retry_count_| is zero when the first attempt is successful.
      // Increase by one for more intuitive metrics.
      ->Add(successive_retry_count_ + 1);

  RecordProbeResult(true);
  ResetState();
}

base::Optional<bool> PreviewsProber::LastProbeWasSuccessful() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Value* cache_entry =
      cached_probe_results_->FindKey(GetCacheKeyForCurrentNetwork());
  if (!cache_entry)
    return base::nullopt;

  base::Optional<PreviewsProberCacheEntry> entry =
      DecodeCacheEntryValue(*cache_entry);
  if (!entry.has_value())
    return base::nullopt;

  // Check if the cache entry should be revalidated.
  if (clock_->Now() >=
      LastModifiedTimeFromCacheEntry(entry.value()) + revalidate_cache_after_) {
    SendNowIfInactive(false);
  }

  return entry.value().is_success();
}

void PreviewsProber::SetOnCompleteCallback(
    PreviewsProberOnCompleteCallback callback) {
  on_complete_callback_ = std::move(callback);
}

void PreviewsProber::RecordProbeResult(bool success) {
  PreviewsProberCacheEntry entry;
  entry.set_is_success(success);
  entry.set_last_modified(
      clock_->Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  base::Optional<base::Value> encoded = EncodeCacheEntryValue(entry);
  if (!encoded.has_value()) {
    NOTREACHED();
    return;
  }

  DictionaryPrefUpdate update(pref_service_, pref_key_);
  update->SetKey(GetCacheKeyForCurrentNetwork(), std::move(encoded.value()));

  if (update.Get()->DictSize() > max_cache_entries_)
    RemoveOldestDictionaryEntry(update.Get());

  cached_probe_results_ = update.Get()->CreateDeepCopy();

  if (on_complete_callback_)
    on_complete_callback_.Run(success);

  base::BooleanHistogram::FactoryGet(
      AppendNameToHistogram(kSuccessHistogram),
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(success);
}

std::string PreviewsProber::GetCacheKeyForCurrentNetwork() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::StringPrintf(
      "%s;%s:%d", GenerateNetworkID(network_connection_tracker_).c_str(),
      url_.host().c_str(), url_.EffectiveIntPort());
}

std::string PreviewsProber::AppendNameToHistogram(
    const std::string& histogram) const {
  return base::StringPrintf("%s.%s", histogram.c_str(), name_.c_str());
}
