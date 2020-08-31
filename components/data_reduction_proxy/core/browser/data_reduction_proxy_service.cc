// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/data_use_measurement/core/data_use_measurement.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_experiments.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace data_reduction_proxy {

namespace {

base::Optional<base::Value> GetSaveDataSavingsPercentEstimateFromFieldTrial() {
  if (!base::FeatureList::IsEnabled(features::kReportSaveDataSavings))
    return base::nullopt;
  const auto origin_savings_estimate_json =
      base::GetFieldTrialParamValueByFeature(features::kReportSaveDataSavings,
                                             "origin_savings_estimate");
  if (origin_savings_estimate_json.empty())
    return base::nullopt;

  auto origin_savings_estimates =
      base::JSONReader::Read(origin_savings_estimate_json);

  UMA_HISTOGRAM_BOOLEAN(
      "DataReductionProxy.ReportSaveDataSavings.ParseResult",
      origin_savings_estimates && origin_savings_estimates->is_dict());

  return origin_savings_estimates;
}

}  // namespace

DataReductionProxyService::DataReductionProxyService(
    DataReductionProxySettings* settings,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<DataStore> store,
    network::NetworkQualityTracker* network_quality_tracker,
    network::NetworkConnectionTracker* network_connection_tracker,
    data_use_measurement::DataUseMeasurement* data_use_measurement,
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::TimeDelta& commit_delay,
    Client client,
    const std::string& channel,
    const std::string& user_agent)
    : url_loader_factory_(std::move(url_loader_factory)),
      settings_(settings),
      prefs_(prefs),
      db_data_owner_(new DBDataOwner(std::move(store))),
      db_task_runner_(db_task_runner),
      network_quality_tracker_(network_quality_tracker),
      network_connection_tracker_(network_connection_tracker),
      data_use_measurement_(data_use_measurement),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      client_(client),
      channel_(channel),
      save_data_savings_estimate_dict_(
          GetSaveDataSavingsPercentEstimateFromFieldTrial()) {
  DCHECK(settings);
  DCHECK(network_quality_tracker_);
  DCHECK(network_connection_tracker_);

  request_options_ =
      std::make_unique<DataReductionProxyRequestOptions>(client_);
  request_options_->Init();
  // It is safe to use base::Unretained here, since it gets executed
  // synchronously on the UI thread, and |this| outlives the caller (since the
  // caller is owned by |this|.
  request_options_->SetUpdateHeaderCallback(
      base::BindRepeating(&DataReductionProxyService::UpdateProxyRequestHeaders,
                          base::Unretained(this)));

  // It is safe to use base::Unretained here, since it gets executed
  // synchronously on the UI thread, and |this| outlives the caller (since the
  // caller is owned by |this|.
  if (params::ForceEnableClientConfigServiceForAllDataSaverUsers()) {
    config_client_ = std::make_unique<DataReductionProxyConfigServiceClient>(
        GetBackoffPolicy(), request_options_.get(), this,
        network_connection_tracker_,
        base::BindRepeating(&DataReductionProxyService::StoreSerializedConfig,
                            base::Unretained(this)));
  }

  if (config_client_)
    config_client_->Initialize(url_loader_factory_);

  ReadPersistedClientConfig();

  db_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&DBDataOwner::InitializeOnDBThread,
                                           db_data_owner_->GetWeakPtr()));
  if (prefs_) {
    compression_stats_.reset(
        new DataReductionProxyCompressionStats(this, prefs_, commit_delay));
  }
  network_quality_tracker_->AddEffectiveConnectionTypeObserver(this);
  network_quality_tracker_->AddRTTAndThroughputEstimatesObserver(this);
  if (data_use_measurement_) {  // null in unit tests.
    data_use_measurement_->AddServicesDataUseObserver(this);
  }

  // TODO(rajendrant): Combine uses of NetworkConnectionTracker within DRP.
  network_connection_tracker_->AddNetworkConnectionObserver(this);
  network_connection_tracker_->GetConnectionType(
      &connection_type_,
      base::BindOnce(&DataReductionProxyService::OnConnectionChanged,
                     GetWeakPtr()));
}

DataReductionProxyService::~DataReductionProxyService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_quality_tracker_->RemoveEffectiveConnectionTypeObserver(this);
  network_quality_tracker_->RemoveRTTAndThroughputEstimatesObserver(this);
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  compression_stats_.reset();
  db_task_runner_->DeleteSoon(FROM_HERE, db_data_owner_.release());
  if (data_use_measurement_) {  // null in unit tests.
    data_use_measurement_->RemoveServicesDataUseObserver(this);
  }
}

void DataReductionProxyService::ReadPersistedClientConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!prefs_)
    return;

  base::Time last_config_retrieval_time =
      base::Time() + base::TimeDelta::FromMicroseconds(prefs_->GetInt64(
                         prefs::kDataReductionProxyLastConfigRetrievalTime));
  base::TimeDelta time_since_last_config_retrieval =
      base::Time::Now() - last_config_retrieval_time;

  // A config older than 24 hours should not be used.
  bool persisted_config_is_expired =
      !last_config_retrieval_time.is_null() &&
      time_since_last_config_retrieval > base::TimeDelta::FromHours(24);

  if (persisted_config_is_expired)
    return;

  const std::string config_value =
      prefs_->GetString(prefs::kDataReductionProxyConfig);

  if (config_value.empty())
    return;

  if (config_client_)
    config_client_->ApplySerializedConfig(config_value);
}

void DataReductionProxyService::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_type_ = type;
}

void DataReductionProxyService::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  effective_connection_type_ = type;
}

void DataReductionProxyService::OnRTTOrThroughputEstimatesComputed(
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    int32_t downstream_throughput_kbps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  http_rtt_ = http_rtt;
}

void DataReductionProxyService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
}

void DataReductionProxyService::UpdateDataUseForHost(int64_t network_bytes,
                                                     int64_t original_bytes,
                                                     const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (compression_stats_) {
    compression_stats_->RecordDataUseByHost(host, network_bytes, original_bytes,
                                            base::Time::Now());
  }
}

void DataReductionProxyService::UpdateContentLengths(
    int64_t data_used,
    int64_t original_size,
    bool data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type,
    const std::string& mime_type,
    bool is_user_traffic,
    data_use_measurement::DataUseUserData::DataUseContentType content_type,
    int32_t service_hash_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (compression_stats_) {
    compression_stats_->RecordDataUseWithMimeType(
        data_used, original_size, data_reduction_proxy_enabled, request_type,
        mime_type, is_user_traffic, content_type, service_hash_code);
  }
}

void DataReductionProxyService::SetUnreachable(bool unreachable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  settings_->SetUnreachable(unreachable);
}

void DataReductionProxyService::SetInt64Pref(const std::string& pref_path,
                                             int64_t value) {
  if (prefs_)
    prefs_->SetInt64(pref_path, value);
}

void DataReductionProxyService::SetStringPref(const std::string& pref_path,
                                              const std::string& value) {
  if (prefs_)
    prefs_->SetString(pref_path, value);
}

void DataReductionProxyService::SetProxyPrefs(bool enabled, bool at_startup) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (config_client_) {
    config_client_->SetEnabled(enabled);
    if (enabled)
      config_client_->RetrieveConfig();
  }

  // If Data Saver is disabled, reset data reduction proxy state.
  if (!enabled) {
    for (auto& client : proxy_config_clients_)
      client->ClearBadProxiesCache();
  }
}

net::EffectiveConnectionType
DataReductionProxyService::GetEffectiveConnectionType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return effective_connection_type_;
}

network::mojom::ConnectionType DataReductionProxyService::GetConnectionType()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return connection_type_;
}

base::Optional<base::TimeDelta> DataReductionProxyService::GetHttpRttEstimate()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return http_rtt_;
}

void DataReductionProxyService::UpdateProxyRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  settings_->SetProxyRequestHeaders(headers);
}

void DataReductionProxyService::UpdatePrefetchProxyHosts(
    const std::vector<GURL>& prefetch_proxies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  settings_->UpdatePrefetchProxyHosts(std::move(prefetch_proxies));
}

void DataReductionProxyService::OnProxyConfigUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DataReductionProxyService::SetIgnoreLongTermBlackListRules(
    bool ignore_long_term_black_list_rules) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  settings_->SetIgnoreLongTermBlackListRules(ignore_long_term_black_list_rules);
}

void DataReductionProxyService::AddCustomProxyConfigClient(
    mojo::Remote<network::mojom::CustomProxyConfigClient> config_client) {
  proxy_config_clients_.Add(std::move(config_client));
}

void DataReductionProxyService::LoadHistoricalDataUsage(
    HistoricalDataUsageCallback load_data_usage_callback) {
  std::unique_ptr<std::vector<DataUsageBucket>> data_usage(
      new std::vector<DataUsageBucket>());
  std::vector<DataUsageBucket>* data_usage_ptr = data_usage.get();
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DBDataOwner::LoadHistoricalDataUsage,
                     db_data_owner_->GetWeakPtr(),
                     base::Unretained(data_usage_ptr)),
      base::BindOnce(std::move(load_data_usage_callback),
                     std::move(data_usage)));
}

void DataReductionProxyService::LoadCurrentDataUsageBucket(
    LoadCurrentDataUsageCallback load_current_data_usage_callback) {
  std::unique_ptr<DataUsageBucket> bucket(new DataUsageBucket());
  DataUsageBucket* bucket_ptr = bucket.get();
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DBDataOwner::LoadCurrentDataUsageBucket,
                     db_data_owner_->GetWeakPtr(),
                     base::Unretained(bucket_ptr)),
      base::BindOnce(std::move(load_current_data_usage_callback),
                     std::move(bucket)));
}

void DataReductionProxyService::StoreCurrentDataUsageBucket(
    std::unique_ptr<DataUsageBucket> current) {
  db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DBDataOwner::StoreCurrentDataUsageBucket,
                     db_data_owner_->GetWeakPtr(), std::move(current)));
}

void DataReductionProxyService::DeleteHistoricalDataUsage() {
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DBDataOwner::DeleteHistoricalDataUsage,
                                db_data_owner_->GetWeakPtr()));
}

void DataReductionProxyService::DeleteBrowsingHistory(const base::Time& start,
                                                      const base::Time& end) {
  DCHECK_LE(start, end);
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DBDataOwner::DeleteBrowsingHistory,
                                db_data_owner_->GetWeakPtr(), start, end));
}

base::WeakPtr<DataReductionProxyService>
DataReductionProxyService::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void DataReductionProxyService::OnServicesDataUse(int32_t service_hash_code,
                                                  int64_t recv_bytes,
                                                  int64_t sent_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (compression_stats_) {
    // Record non-content initiated traffic to the Other bucket for data saver
    // site-breakdown.
    compression_stats_->RecordDataUseByHost(
        util::GetSiteBreakdownOtherHostName(), sent_bytes, sent_bytes,
        base::Time::Now());
    compression_stats_->RecordDataUseByHost(
        util::GetSiteBreakdownOtherHostName(), recv_bytes, recv_bytes,
        base::Time::Now());
    compression_stats_->RecordDataUseWithMimeType(
        recv_bytes, recv_bytes, settings_->IsDataReductionProxyEnabled(), HTTPS,
        std::string(), false, data_use_measurement::DataUseUserData::OTHER,
        service_hash_code);
  }
}

void DataReductionProxyService::StoreSerializedConfig(
    const std::string& serialized_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(params::ForceEnableClientConfigServiceForAllDataSaverUsers());
  SetStringPref(prefs::kDataReductionProxyConfig, serialized_config);
  SetInt64Pref(prefs::kDataReductionProxyLastConfigRetrievalTime,
               (base::Time::Now() - base::Time()).InMicroseconds());
}

void DataReductionProxyService::SetDependenciesForTesting(
    std::unique_ptr<DataReductionProxyRequestOptions> request_options,
    std::unique_ptr<DataReductionProxyConfigServiceClient> config_client) {
  request_options_ = std::move(request_options);
  request_options_->SetUpdateHeaderCallback(
      base::BindRepeating(&DataReductionProxyService::UpdateProxyRequestHeaders,
                          base::Unretained(this)));

  config_client_ = std::move(config_client);

  if (config_client_)
    config_client_->Initialize(url_loader_factory_);
}

double DataReductionProxyService::GetSaveDataSavingsPercentEstimate(
    const std::string& origin) const {
  if (origin.empty() || !save_data_savings_estimate_dict_ ||
      !save_data_savings_estimate_dict_->is_dict()) {
    return 0;
  }
  const auto savings_percent =
      save_data_savings_estimate_dict_->FindDoubleKey(origin);
  if (!savings_percent)
    return 0;
  return *savings_percent;
}

}  // namespace data_reduction_proxy
