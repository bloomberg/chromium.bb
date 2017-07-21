// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "components/client_update_protocol/ecdsa.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_context_getter.h"

namespace network_time {

// Network time queries are enabled on all desktop platforms except ChromeOS,
// which uses tlsdated to set the system time.
#if defined(OS_ANDROID) || defined(OS_CHROMEOS) || defined(OS_IOS)
const base::Feature kNetworkTimeServiceQuerying{
    "NetworkTimeServiceQuerying", base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kNetworkTimeServiceQuerying{
    "NetworkTimeServiceQuerying", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

namespace {

// Time updates happen in two ways. First, other components may call
// UpdateNetworkTime() if they happen to obtain the time securely.  This will
// likely be deprecated in favor of the second way, which is scheduled time
// queries issued by NetworkTimeTracker itself.
//
// On startup, the clock state may be read from a pref.  (This, too, may be
// deprecated.)  After that, the time is checked every
// |kCheckTimeIntervalSeconds|.  A "check" means the possibility, but not the
// certainty, of a time query.  A time query may be issued at random, or if the
// network time is believed to have become inaccurate.
//
// After issuing a query, the next check will not happen until
// |kBackoffMinutes|.  This delay is doubled in the event of an error.

// Minimum number of minutes between time queries.
const uint32_t kBackoffMinutes = 60;

// Number of seconds between time checks.  This may be overridden via Variations
// Service.
// Note that a "check" is not necessarily a network time query!
const uint32_t kCheckTimeIntervalSeconds = 360;

// Probability that a check will randomly result in a query.  This may
// be overridden via Variations Service.  Checks are made every
// |kCheckTimeIntervalSeconds|.  The default values are chosen with
// the goal of a high probability that a query will be issued every 24
// hours.
const float kRandomQueryProbability = .012f;

// Number of time measurements performed in a given network time calculation.
const uint32_t kNumTimeMeasurements = 7;

// Amount of divergence allowed between wall clock and tick clock.
const uint32_t kClockDivergenceSeconds = 60;

// Maximum time lapse before deserialized data are considered stale.
const uint32_t kSerializedDataMaxAgeDays = 7;

// Name of a pref that stores the wall clock time, via |ToJsTime|.
const char kPrefTime[] = "local";

// Name of a pref that stores the tick clock time, via |ToInternalValue|.
const char kPrefTicks[] = "ticks";

// Name of a pref that stores the time uncertainty, via |ToInternalValue|.
const char kPrefUncertainty[] = "uncertainty";

// Name of a pref that stores the network time via |ToJsTime|.
const char kPrefNetworkTime[] = "network";

// Time server's maximum allowable clock skew, in seconds.  (This is a property
// of the time server that we happen to know.  It's unlikely that it would ever
// be that badly wrong, but all the same it's included here to document the very
// rough nature of the time service provided by this class.)
const uint32_t kTimeServerMaxSkewSeconds = 10;

const char kTimeServiceURL[] = "http://clients2.google.com/time/1/current";

const char kVariationsServiceCheckTimeIntervalSeconds[] =
    "CheckTimeIntervalSeconds";
const char kVariationsServiceRandomQueryProbability[] =
    "RandomQueryProbability";

// This parameter can have three values:
//
// - "background-only": Time queries will be issued in the background as
//   needed (when the clock loses sync), but on-demand time queries will
//   not be issued (i.e. StartTimeFetch() will not start time queries.)
//
// - "on-demand-only": Time queries will not be issued except when
//   StartTimeFetch() is called. This is the default value.
//
// - "background-and-on-demand": Time queries will be issued both in the
//   background as needed and also on-demand.
const char kVariationsServiceFetchBehavior[] = "FetchBehavior";

// This is an ECDSA prime256v1 named-curve key.
const int kKeyVersion = 1;
const uint8_t kKeyPubBytes[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0xeb, 0xd8, 0xad, 0x0b, 0x8f, 0x75, 0xe8, 0x84, 0x36,
    0x23, 0x48, 0x14, 0x24, 0xd3, 0x93, 0x42, 0x25, 0x43, 0xc1, 0xde, 0x36,
    0x29, 0xc6, 0x95, 0xca, 0xeb, 0x28, 0x85, 0xff, 0x09, 0xdc, 0x08, 0xec,
    0x45, 0x74, 0x6e, 0x4b, 0xc3, 0xa5, 0xfd, 0x8a, 0x2f, 0x02, 0xa0, 0x4b,
    0xc3, 0xc6, 0xa4, 0x7b, 0xa4, 0x41, 0xfc, 0xa7, 0x02, 0x54, 0xab, 0xe3,
    0xe4, 0xb1, 0x00, 0xf5, 0xd5, 0x09, 0x11};

std::string GetServerProof(const net::URLFetcher* source) {
  const net::HttpResponseHeaders* response_headers =
      source->GetResponseHeaders();
  if (!response_headers) {
    return std::string();
  }
  std::string proof;
  return response_headers->EnumerateHeader(nullptr, "x-cup-server-proof",
                                           &proof)
             ? proof
             : std::string();
}

// Limits the amount of data that will be buffered from the server's response.
class SizeLimitingStringWriter : public net::URLFetcherStringWriter {
 public:
  explicit SizeLimitingStringWriter(size_t limit) : limit_(limit) {}

  int Write(net::IOBuffer* buffer,
            int num_bytes,
            const net::CompletionCallback& callback) override {
    if (data().length() + num_bytes > limit_) {
      return net::ERR_FILE_TOO_BIG;
    }
    return net::URLFetcherStringWriter::Write(buffer, num_bytes, callback);
  }

 private:
  size_t limit_;
};

base::TimeDelta CheckTimeInterval() {
  int64_t seconds;
  const std::string param = variations::GetVariationParamValueByFeature(
      kNetworkTimeServiceQuerying, kVariationsServiceCheckTimeIntervalSeconds);
  if (!param.empty() && base::StringToInt64(param, &seconds) && seconds > 0) {
    return base::TimeDelta::FromSeconds(seconds);
  }
  return base::TimeDelta::FromSeconds(kCheckTimeIntervalSeconds);
}

double RandomQueryProbability() {
  double probability;
  const std::string param = variations::GetVariationParamValueByFeature(
      kNetworkTimeServiceQuerying, kVariationsServiceRandomQueryProbability);
  if (!param.empty() && base::StringToDouble(param, &probability) &&
      probability >= 0.0 && probability <= 1.0) {
    return probability;
  }
  return kRandomQueryProbability;
}

void RecordFetchValidHistogram(bool valid) {
  UMA_HISTOGRAM_BOOLEAN("NetworkTimeTracker.UpdateTimeFetchValid", valid);
}

}  // namespace

// static
void NetworkTimeTracker::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNetworkTimeMapping,
                                   base::MakeUnique<base::DictionaryValue>());
  registry->RegisterBooleanPref(prefs::kNetworkTimeQueriesEnabled, true);
}

NetworkTimeTracker::NetworkTimeTracker(
    std::unique_ptr<base::Clock> clock,
    std::unique_ptr<base::TickClock> tick_clock,
    PrefService* pref_service,
    scoped_refptr<net::URLRequestContextGetter> getter)
    : server_url_(kTimeServiceURL),
      max_response_size_(1024),
      backoff_(base::TimeDelta::FromMinutes(kBackoffMinutes)),
      getter_(std::move(getter)),
      clock_(std::move(clock)),
      tick_clock_(std::move(tick_clock)),
      pref_service_(pref_service),
      time_query_completed_(false) {
  const base::DictionaryValue* time_mapping =
      pref_service_->GetDictionary(prefs::kNetworkTimeMapping);
  double time_js = 0;
  double ticks_js = 0;
  double network_time_js = 0;
  double uncertainty_js = 0;
  if (time_mapping->GetDouble(kPrefTime, &time_js) &&
      time_mapping->GetDouble(kPrefTicks, &ticks_js) &&
      time_mapping->GetDouble(kPrefUncertainty, &uncertainty_js) &&
      time_mapping->GetDouble(kPrefNetworkTime, &network_time_js)) {
    time_at_last_measurement_ = base::Time::FromJsTime(time_js);
    ticks_at_last_measurement_ = base::TimeTicks::FromInternalValue(
        static_cast<int64_t>(ticks_js));
    network_time_uncertainty_ = base::TimeDelta::FromInternalValue(
        static_cast<int64_t>(uncertainty_js));
    network_time_at_last_measurement_ = base::Time::FromJsTime(network_time_js);
  }
  base::Time now = clock_->Now();
  if (ticks_at_last_measurement_ > tick_clock_->NowTicks() ||
      time_at_last_measurement_ > now ||
      now - time_at_last_measurement_ >
          base::TimeDelta::FromDays(kSerializedDataMaxAgeDays)) {
    // Drop saved mapping if either clock has run backward, or the data are too
    // old.
    pref_service_->ClearPref(prefs::kNetworkTimeMapping);
    network_time_at_last_measurement_ = base::Time();  // Reset.
  }

  base::StringPiece public_key = {reinterpret_cast<const char*>(kKeyPubBytes),
                                  sizeof(kKeyPubBytes)};
  query_signer_ =
      client_update_protocol::Ecdsa::Create(kKeyVersion, public_key);

  QueueCheckTime(base::TimeDelta::FromSeconds(0));
}

NetworkTimeTracker::~NetworkTimeTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void NetworkTimeTracker::UpdateNetworkTime(base::Time network_time,
                                           base::TimeDelta resolution,
                                           base::TimeDelta latency,
                                           base::TimeTicks post_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Network time updating to "
           << base::UTF16ToUTF8(
                  base::TimeFormatFriendlyDateAndTime(network_time));
  // Update network time on every request to limit dependency on ticks lag.
  // TODO(mad): Find a heuristic to avoid augmenting the
  // network_time_uncertainty_ too much by a particularly long latency.
  // Maybe only update when the the new time either improves in accuracy or
  // drifts too far from |network_time_at_last_measurement_|.
  network_time_at_last_measurement_ = network_time;

  // Calculate the delay since the network time was received.
  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  base::TimeDelta task_delay = now_ticks - post_time;
  DCHECK_GE(task_delay.InMilliseconds(), 0);
  DCHECK_GE(latency.InMilliseconds(), 0);
  // Estimate that the time was set midway through the latency time.
  base::TimeDelta offset = task_delay + latency / 2;
  ticks_at_last_measurement_ = now_ticks - offset;
  time_at_last_measurement_ = clock_->Now() - offset;

  // Can't assume a better time than the resolution of the given time and the
  // ticks measurements involved, each with their own uncertainty.  1 & 2 are
  // the ones used to compute the latency, 3 is the Now() from when this task
  // was posted, 4 and 5 are the Now() and NowTicks() above, and 6 and 7 will be
  // the Now() and NowTicks() in GetNetworkTime().
  network_time_uncertainty_ =
      resolution + latency + kNumTimeMeasurements *
      base::TimeDelta::FromMilliseconds(kTicksResolutionMs);

  base::DictionaryValue time_mapping;
  time_mapping.SetDouble(kPrefTime, time_at_last_measurement_.ToJsTime());
  time_mapping.SetDouble(kPrefTicks, static_cast<double>(
      ticks_at_last_measurement_.ToInternalValue()));
  time_mapping.SetDouble(kPrefUncertainty, static_cast<double>(
      network_time_uncertainty_.ToInternalValue()));
  time_mapping.SetDouble(kPrefNetworkTime,
      network_time_at_last_measurement_.ToJsTime());
  pref_service_->Set(prefs::kNetworkTimeMapping, time_mapping);
}

bool NetworkTimeTracker::AreTimeFetchesEnabled() const {
  return base::FeatureList::IsEnabled(kNetworkTimeServiceQuerying);
}

NetworkTimeTracker::FetchBehavior NetworkTimeTracker::GetFetchBehavior() const {
  const std::string param = variations::GetVariationParamValueByFeature(
      kNetworkTimeServiceQuerying, kVariationsServiceFetchBehavior);
  if (param == "background-only") {
    return FETCHES_IN_BACKGROUND_ONLY;
  } else if (param == "on-demand-only") {
    return FETCHES_ON_DEMAND_ONLY;
  } else if (param == "background-and-on-demand") {
    return FETCHES_IN_BACKGROUND_AND_ON_DEMAND;
  }
  return FETCHES_ON_DEMAND_ONLY;
}

void NetworkTimeTracker::SetTimeServerURLForTesting(const GURL& url) {
  server_url_ = url;
}

GURL NetworkTimeTracker::GetTimeServerURLForTesting() const {
  return server_url_;
}

void NetworkTimeTracker::SetMaxResponseSizeForTesting(size_t limit) {
  max_response_size_ = limit;
}

void NetworkTimeTracker::SetPublicKeyForTesting(const base::StringPiece& key) {
  query_signer_ = client_update_protocol::Ecdsa::Create(kKeyVersion, key);
}

bool NetworkTimeTracker::QueryTimeServiceForTesting() {
  CheckTime();
  return time_fetcher_ != nullptr;
}

void NetworkTimeTracker::WaitForFetchForTesting(uint32_t nonce) {
  query_signer_->OverrideNonceForTesting(kKeyVersion, nonce);
  base::RunLoop run_loop;
  fetch_completion_callbacks_.push_back(run_loop.QuitClosure());
  run_loop.Run();
}

void NetworkTimeTracker::OverrideNonceForTesting(uint32_t nonce) {
  query_signer_->OverrideNonceForTesting(kKeyVersion, nonce);
}

base::TimeDelta NetworkTimeTracker::GetTimerDelayForTesting() const {
  DCHECK(timer_.IsRunning());
  return timer_.GetCurrentDelay();
}

NetworkTimeTracker::NetworkTimeResult NetworkTimeTracker::GetNetworkTime(
    base::Time* network_time,
    base::TimeDelta* uncertainty) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(network_time);
  if (network_time_at_last_measurement_.is_null()) {
    if (time_query_completed_) {
      // Time query attempts have been made in the past and failed.
      if (time_fetcher_) {
        // A fetch (not the first attempt) is in progress.
        return NETWORK_TIME_SUBSEQUENT_SYNC_PENDING;
      }
      return NETWORK_TIME_NO_SUCCESSFUL_SYNC;
    }
    // No time queries have happened yet.
    if (time_fetcher_) {
      return NETWORK_TIME_FIRST_SYNC_PENDING;
    }
    return NETWORK_TIME_NO_SYNC_ATTEMPT;
  }

  DCHECK(!ticks_at_last_measurement_.is_null());
  DCHECK(!time_at_last_measurement_.is_null());
  base::TimeDelta tick_delta =
      tick_clock_->NowTicks() - ticks_at_last_measurement_;
  base::TimeDelta time_delta = clock_->Now() - time_at_last_measurement_;
  if (time_delta.InMilliseconds() < 0) {  // Has wall clock run backward?
    DVLOG(1) << "Discarding network time due to wall clock running backward";
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "NetworkTimeTracker.WallClockRanBackwards", time_delta.magnitude(),
        base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(7), 50);
    network_time_at_last_measurement_ = base::Time();
    return NETWORK_TIME_SYNC_LOST;
  }
  // Now we know that both |tick_delta| and |time_delta| are positive.
  base::TimeDelta divergence = tick_delta - time_delta;
  if (divergence.magnitude() >
      base::TimeDelta::FromSeconds(kClockDivergenceSeconds)) {
    // Most likely either the machine has suspended, or the wall clock has been
    // reset.
    DVLOG(1) << "Discarding network time due to clocks diverging";

    // The below histograms do not use |kClockDivergenceSeconds| as the
    // lower-bound, so that |kClockDivergenceSeconds| can be changed
    // without causing the buckets to change and making data from
    // old/new clients incompatible.
    if (divergence.InMilliseconds() < 0) {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NetworkTimeTracker.ClockDivergence.Negative", divergence.magnitude(),
          base::TimeDelta::FromSeconds(60), base::TimeDelta::FromDays(7), 50);
    } else {
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "NetworkTimeTracker.ClockDivergence.Positive", divergence.magnitude(),
          base::TimeDelta::FromSeconds(60), base::TimeDelta::FromDays(7), 50);
    }
    network_time_at_last_measurement_ = base::Time();
    return NETWORK_TIME_SYNC_LOST;
  }
  *network_time = network_time_at_last_measurement_ + tick_delta;
  if (uncertainty) {
    *uncertainty = network_time_uncertainty_ + divergence;
  }
  return NETWORK_TIME_AVAILABLE;
}

bool NetworkTimeTracker::StartTimeFetch(const base::Closure& closure) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FetchBehavior behavior = GetFetchBehavior();
  if (behavior != FETCHES_ON_DEMAND_ONLY &&
      behavior != FETCHES_IN_BACKGROUND_AND_ON_DEMAND) {
    return false;
  }

  // Enqueue the callback before calling CheckTime(), so that if
  // CheckTime() completes synchronously, the callback gets called.
  fetch_completion_callbacks_.push_back(closure);

  // If a time query is already in progress, do not start another one.
  if (time_fetcher_) {
    return true;
  }

  // Cancel any fetches that are scheduled for the future, and try to
  // start one now.
  timer_.Stop();
  CheckTime();

  // CheckTime() does not necessarily start a fetch; for example, time
  // queries might be disabled or network time might already be
  // available.
  if (!time_fetcher_) {
    // If no query is in progress, no callbacks need to be called.
    fetch_completion_callbacks_.clear();
    return false;
  }
  return true;
}

void NetworkTimeTracker::CheckTime() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If NetworkTimeTracker is waking up after a backoff, this will reset the
  // timer to its default faster frequency.
  QueueCheckTime(CheckTimeInterval());

  if (!ShouldIssueTimeQuery()) {
    return;
  }

  std::string query_string;
  query_signer_->SignRequest(nullptr, &query_string);
  GURL url = server_url_;
  GURL::Replacements replacements;
  replacements.SetQueryStr(query_string);
  url = url.ReplaceComponents(replacements);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("network_time_component", R"(
        semantics {
          sender: "Network Time Component"
          description:
            "Sends a request to a Google server to retrieve the current "
            "timestamp."
          trigger:
            "A request can be sent to retrieve the current time when the user "
            "encounters an SSL date error, or in the background if Chromium "
            "determines that it doesn't have an accurate timestamp."
          data: "None"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            BrowserNetworkTimeQueriesEnabled {
                BrowserNetworkTimeQueriesEnabled: false
            }
          }
        })");
  // This cancels any outstanding fetch.
  time_fetcher_ = net::URLFetcher::Create(url, net::URLFetcher::GET, this,
                                          traffic_annotation);
  if (!time_fetcher_) {
    DVLOG(1) << "tried to make fetch happen; failed";
    return;
  }
  data_use_measurement::DataUseUserData::AttachToFetcher(
      time_fetcher_.get(),
      data_use_measurement::DataUseUserData::NETWORK_TIME_TRACKER);
  time_fetcher_->SaveResponseWithWriter(
      std::unique_ptr<net::URLFetcherResponseWriter>(
          new SizeLimitingStringWriter(max_response_size_)));
  DCHECK(getter_);
  time_fetcher_->SetRequestContext(getter_.get());
  // Not expecting any cookies, but just in case.
  time_fetcher_->SetLoadFlags(net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
                              net::LOAD_DO_NOT_SAVE_COOKIES |
                              net::LOAD_DO_NOT_SEND_COOKIES |
                              net::LOAD_DO_NOT_SEND_AUTH_DATA);

  time_fetcher_->Start();
  fetch_started_ = tick_clock_->NowTicks();

  timer_.Stop();  // Restarted in OnURLFetchComplete().
}

bool NetworkTimeTracker::UpdateTimeFromResponse() {
  if (time_fetcher_->GetStatus().status() != net::URLRequestStatus::SUCCESS ||
      time_fetcher_->GetResponseCode() != 200) {
    time_query_completed_ = true;
    DVLOG(1) << "fetch failed, status=" << time_fetcher_->GetStatus().status()
             << ",code=" << time_fetcher_->GetResponseCode();
    // The error code is negated because net errors are negative, but
    // the corresponding histogram enum is positive.
    UMA_HISTOGRAM_SPARSE_SLOWLY("NetworkTimeTracker.UpdateTimeFetchFailed",
                                -time_fetcher_->GetStatus().error());
    return false;
  }

  std::string response_body;
  if (!time_fetcher_->GetResponseAsString(&response_body)) {
    DVLOG(1) << "failed to get response";
    return false;
  }
  DCHECK(query_signer_);
  if (!query_signer_->ValidateResponse(response_body,
                                       GetServerProof(time_fetcher_.get()))) {
    DVLOG(1) << "invalid signature";
    RecordFetchValidHistogram(false);
    return false;
  }
  response_body = response_body.substr(5);  // Skips leading )]}'\n
  std::unique_ptr<base::Value> value = base::JSONReader::Read(response_body);
  if (!value) {
    DVLOG(1) << "bad JSON";
    RecordFetchValidHistogram(false);
    return false;
  }
  const base::DictionaryValue* dict;
  if (!value->GetAsDictionary(&dict)) {
    DVLOG(1) << "not a dictionary";
    RecordFetchValidHistogram(false);
    return false;
  }
  double current_time_millis;
  if (!dict->GetDouble("current_time_millis", &current_time_millis)) {
    DVLOG(1) << "no current_time_millis";
    RecordFetchValidHistogram(false);
    return false;
  }

  RecordFetchValidHistogram(true);

  // There is a "server_nonce" key here too, but it serves no purpose other than
  // to make the server's response unpredictable.
  base::Time current_time = base::Time::FromJsTime(current_time_millis);
  base::TimeDelta resolution =
      base::TimeDelta::FromMilliseconds(1) +
      base::TimeDelta::FromSeconds(kTimeServerMaxSkewSeconds);

  // Record histograms for the latency of the time query and the time delta
  // between time fetches.
  base::TimeDelta latency = tick_clock_->NowTicks() - fetch_started_;
  UMA_HISTOGRAM_TIMES("NetworkTimeTracker.TimeQueryLatency", latency);
  if (!last_fetched_time_.is_null()) {
    UMA_HISTOGRAM_CUSTOM_TIMES("NetworkTimeTracker.TimeBetweenFetches",
                               current_time - last_fetched_time_,
                               base::TimeDelta::FromHours(1),
                               base::TimeDelta::FromDays(7), 50);
  }
  last_fetched_time_ = current_time;

  UpdateNetworkTime(current_time, resolution, latency, tick_clock_->NowTicks());
  return true;
}

void NetworkTimeTracker::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(time_fetcher_);
  DCHECK_EQ(source, time_fetcher_.get());

  time_query_completed_ = true;

  // After completion of a query, whether succeeded or failed, go to sleep for a
  // long time.
  if (!UpdateTimeFromResponse()) {  // On error, back off.
    if (backoff_ < base::TimeDelta::FromDays(2)) {
      backoff_ *= 2;
    }
  } else {
    backoff_ = base::TimeDelta::FromMinutes(kBackoffMinutes);
  }
  QueueCheckTime(backoff_);
  time_fetcher_.reset();

  // Clear |fetch_completion_callbacks_| before running any of them,
  // because a callback could call StartTimeFetch() to enqueue another
  // callback.
  std::vector<base::Closure> callbacks = fetch_completion_callbacks_;
  fetch_completion_callbacks_.clear();
  for (const auto& callback : callbacks) {
    callback.Run();
  }
}

void NetworkTimeTracker::QueueCheckTime(base::TimeDelta delay) {
  // Check if the user is opted in to background time fetches.
  FetchBehavior behavior = GetFetchBehavior();
  if (behavior == FETCHES_IN_BACKGROUND_ONLY ||
      behavior == FETCHES_IN_BACKGROUND_AND_ON_DEMAND) {
    timer_.Start(FROM_HERE, delay, this, &NetworkTimeTracker::CheckTime);
  }
}

bool NetworkTimeTracker::ShouldIssueTimeQuery() {
  // Do not query the time service if not enabled via Variations Service.
  if (!AreTimeFetchesEnabled()) {
    return false;
  }

  // Do not query the time service if queries are disabled by policy.
  if (!pref_service_->GetBoolean(prefs::kNetworkTimeQueriesEnabled)) {
    return false;
  }

  // If GetNetworkTime() does not return NETWORK_TIME_AVAILABLE,
  // synchronization has been lost and a query is needed.
  base::Time network_time;
  if (GetNetworkTime(&network_time, nullptr) != NETWORK_TIME_AVAILABLE) {
    return true;
  }

  // Otherwise, make the decision at random.
  return base::RandDouble() < RandomQueryProbability();
}

}  // namespace network_time
