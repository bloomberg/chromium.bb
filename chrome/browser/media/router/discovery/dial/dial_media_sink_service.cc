// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/media_router/media_source_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace media_router {

DialMediaSinkService::DialMediaSinkService()
    : impl_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
      weak_ptr_factory_(this) {}

DialMediaSinkService::~DialMediaSinkService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DialMediaSinkService::Start(
    const OnSinksDiscoveredCallback& sink_discovery_cb,
    const OnDialSinkAddedCallback& dial_sink_added_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!impl_);

  OnSinksDiscoveredCallback sink_discovery_cb_impl = base::BindRepeating(
      &RunSinksDiscoveredCallbackOnSequence,
      base::SequencedTaskRunnerHandle::Get(),
      base::BindRepeating(&DialMediaSinkService::RunSinksDiscoveredCallback,
                          weak_ptr_factory_.GetWeakPtr(), sink_discovery_cb));

  OnAvailableSinksUpdatedCallback available_sinks_updated_cb_impl =
      base::BindRepeating(
          &RunAvailableSinksUpdatedCallbackOnSequence,
          base::SequencedTaskRunnerHandle::Get(),
          base::BindRepeating(
              &DialMediaSinkService::RunAvailableSinksUpdatedCallback,
              weak_ptr_factory_.GetWeakPtr()));

  impl_ = CreateImpl(sink_discovery_cb_impl, dial_sink_added_cb,
                     available_sinks_updated_cb_impl);

  impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DialMediaSinkServiceImpl::Start,
                                base::Unretained(impl_.get())));
}

void DialMediaSinkService::OnUserGesture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DialMediaSinkServiceImpl::OnUserGesture,
                                base::Unretained(impl_.get())));
}

DialMediaSinkService::SinkQueryByAppSubscription
DialMediaSinkService::StartMonitoringAvailableSinksForApp(
    const std::string& app_name,
    const SinkQueryByAppCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& sink_list = sinks_by_app_name_[app_name];
  if (!sink_list) {
    // Register first observer for |app_name|.
    sink_list = std::make_unique<SinkListByAppName>();
    impl_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DialMediaSinkServiceImpl::StartMonitoringAvailableSinksForApp,
            base::Unretained(impl_.get()), app_name));
    sink_list->callbacks.set_removal_callback(base::BindRepeating(
        &DialMediaSinkService::OnAvailableSinksUpdatedCallbackRemoved,
        base::Unretained(this), app_name));
  }

  return sink_list->callbacks.Add(callback);
}

std::vector<MediaSinkInternal> DialMediaSinkService::GetCachedAvailableSinks(
    const std::string& app_name) {
  const auto& sinks_it = sinks_by_app_name_.find(app_name);
  if (sinks_it == sinks_by_app_name_.end())
    return std::vector<MediaSinkInternal>();

  return sinks_it->second->cached_sinks;
}

DialMediaSinkService::SinkListByAppName::SinkListByAppName() = default;
DialMediaSinkService::SinkListByAppName::~SinkListByAppName() = default;

std::unique_ptr<DialMediaSinkServiceImpl, base::OnTaskRunnerDeleter>
DialMediaSinkService::CreateImpl(
    const OnSinksDiscoveredCallback& sink_discovery_cb,
    const OnDialSinkAddedCallback& dial_sink_added_cb,
    const OnAvailableSinksUpdatedCallback& available_sinks_updated_cb) {
  // Clone the connector so it can be used on the IO thread.
  std::unique_ptr<service_manager::Connector> connector =
      content::ServiceManagerConnection::GetForProcess()
          ->GetConnector()
          ->Clone();

  // Note: The SequencedTaskRunner needs to be IO thread because DialRegistry
  // runs on IO thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO);
  return std::unique_ptr<DialMediaSinkServiceImpl, base::OnTaskRunnerDeleter>(
      new DialMediaSinkServiceImpl(std::move(connector), sink_discovery_cb,
                                   dial_sink_added_cb,
                                   available_sinks_updated_cb, task_runner),
      base::OnTaskRunnerDeleter(task_runner));
}

void DialMediaSinkService::RunSinksDiscoveredCallback(
    const OnSinksDiscoveredCallback& sinks_discovered_cb,
    std::vector<MediaSinkInternal> sinks) {
  sinks_discovered_cb.Run(std::move(sinks));
}

void DialMediaSinkService::RunAvailableSinksUpdatedCallback(
    const std::string& app_name,
    std::vector<MediaSinkInternal> available_sinks) {
  const auto& sinks_it = sinks_by_app_name_.find(app_name);
  if (sinks_it == sinks_by_app_name_.end())
    return;

  sinks_it->second->callbacks.Notify(app_name, available_sinks);
  sinks_it->second->cached_sinks = std::move(available_sinks);
}

void DialMediaSinkService::OnAvailableSinksUpdatedCallbackRemoved(
    const std::string& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& sinks_it = sinks_by_app_name_.find(app_name);
  if (sinks_it == sinks_by_app_name_.end())
    return;

  // Other profile is still monitoring |app_name|.
  if (!sinks_it->second->callbacks.empty())
    return;

  impl_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DialMediaSinkServiceImpl::StopMonitoringAvailableSinksForApp,
          base::Unretained(impl_.get()), app_name));

  sinks_by_app_name_.erase(app_name);
}

}  // namespace media_router
