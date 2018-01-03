// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_checker.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_builder.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client.h"
#include "components/update_client/updater_state.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

namespace update_client {

namespace {

// Returns true if at least one item requires network encryption.
bool IsEncryptionRequired(const IdToComponentPtrMap& components) {
  for (const auto& item : components) {
    const auto& component = item.second;
    if (component->crx_component().requires_network_encryption)
      return true;
  }
  return false;
}


class UpdateCheckerImpl : public UpdateChecker {
 public:
  UpdateCheckerImpl(const scoped_refptr<Configurator>& config,
                    PersistedData* metadata);
  ~UpdateCheckerImpl() override;

  // Overrides for UpdateChecker.
  void CheckForUpdates(const std::vector<std::string>& ids_checked,
                       const IdToComponentPtrMap& components,
                       const std::string& additional_attributes,
                       bool enabled_component_updates,
                       UpdateCheckCallback update_check_callback) override;

 private:
  void ReadUpdaterStateAttributes();
  void CheckForUpdatesHelper(const IdToComponentPtrMap& components,
                             const std::string& additional_attributes,
                             bool enabled_component_updates);
  void OnRequestSenderComplete(const IdToComponentPtrMap& components,
                               int error,
                               const std::string& response,
                               int retry_after_sec);
  void UpdateCheckSucceeded(const IdToComponentPtrMap& components,
                            const ProtocolParser::Results& results,
                            int retry_after_sec);
  void UpdateCheckFailed(const IdToComponentPtrMap& components,
                         int error,
                         int retry_after_sec);

  base::ThreadChecker thread_checker_;

  const scoped_refptr<Configurator> config_;
  PersistedData* metadata_ = nullptr;
  std::vector<std::string> ids_checked_;
  UpdateCheckCallback update_check_callback_;
  std::unique_ptr<UpdaterState::Attributes> updater_state_attributes_;
  std::unique_ptr<RequestSender> request_sender_;

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckerImpl);
};

UpdateCheckerImpl::UpdateCheckerImpl(const scoped_refptr<Configurator>& config,
                                     PersistedData* metadata)
    : config_(config), metadata_(metadata) {}

UpdateCheckerImpl::~UpdateCheckerImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void UpdateCheckerImpl::CheckForUpdates(
    const std::vector<std::string>& ids_checked,
    const IdToComponentPtrMap& components,
    const std::string& additional_attributes,
    bool enabled_component_updates,
    UpdateCheckCallback update_check_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ids_checked_ = ids_checked;
  update_check_callback_ = std::move(update_check_callback);

  base::PostTaskWithTraitsAndReply(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&UpdateCheckerImpl::ReadUpdaterStateAttributes,
                     base::Unretained(this)),
      base::BindOnce(&UpdateCheckerImpl::CheckForUpdatesHelper,
                     base::Unretained(this), base::ConstRef(components),
                     additional_attributes, enabled_component_updates));
}

// This function runs on the blocking pool task runner.
void UpdateCheckerImpl::ReadUpdaterStateAttributes() {
  const bool is_machine_install = !config_->IsPerUserInstall();
  updater_state_attributes_ = UpdaterState::GetState(is_machine_install);
}

void UpdateCheckerImpl::CheckForUpdatesHelper(
    const IdToComponentPtrMap& components,
    const std::string& additional_attributes,
    bool enabled_component_updates) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto urls(config_->UpdateUrl());
  if (IsEncryptionRequired(components))
    RemoveUnsecureUrls(&urls);

  request_sender_ = std::make_unique<RequestSender>(config_);
  request_sender_->Send(
      config_->EnabledCupSigning(),
      BuildUpdateCheckRequest(*config_, ids_checked_, components, metadata_,
                              additional_attributes, enabled_component_updates,
                              updater_state_attributes_),
      urls,
      base::BindOnce(&UpdateCheckerImpl::OnRequestSenderComplete,
                     base::Unretained(this), base::ConstRef(components)));
}

void UpdateCheckerImpl::OnRequestSenderComplete(
    const IdToComponentPtrMap& components,
    int error,
    const std::string& response,
    int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (error) {
    VLOG(1) << "RequestSender failed " << error;
    UpdateCheckFailed(components, error, retry_after_sec);
    return;
  }

  ProtocolParser update_response;
  if (!update_response.Parse(response)) {
    VLOG(1) << "Parse failed " << update_response.errors();
    UpdateCheckFailed(components, -1, retry_after_sec);
    return;
  }

  DCHECK_EQ(0, error);
  UpdateCheckSucceeded(components, update_response.results(), retry_after_sec);
}

void UpdateCheckerImpl::UpdateCheckSucceeded(
    const IdToComponentPtrMap& components,
    const ProtocolParser::Results& results,
    int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const int daynum = results.daystart_elapsed_days;
  if (daynum != ProtocolParser::kNoDaystart) {
    metadata_->SetDateLastActive(ids_checked_, daynum);
    metadata_->SetDateLastRollCall(ids_checked_, daynum);
  }
  for (const auto& result : results.list) {
    auto entry = result.cohort_attrs.find(ProtocolParser::Result::kCohort);
    if (entry != result.cohort_attrs.end())
      metadata_->SetCohort(result.extension_id, entry->second);
    entry = result.cohort_attrs.find(ProtocolParser::Result::kCohortName);
    if (entry != result.cohort_attrs.end())
      metadata_->SetCohortName(result.extension_id, entry->second);
    entry = result.cohort_attrs.find(ProtocolParser::Result::kCohortHint);
    if (entry != result.cohort_attrs.end())
      metadata_->SetCohortHint(result.extension_id, entry->second);
  }

  for (const auto& result : results.list) {
    const auto& id = result.extension_id;
    const auto it = components.find(id);
    if (it != components.end())
      it->second->SetParseResult(result);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(update_check_callback_), 0, retry_after_sec));
}

void UpdateCheckerImpl::UpdateCheckFailed(const IdToComponentPtrMap& components,
                                          int error,
                                          int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(0, error);
  for (const auto& item : components) {
    DCHECK(item.second);
    Component& component = *item.second;
    component.set_update_check_error(error);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(update_check_callback_), error,
                                retry_after_sec));
}

}  // namespace

std::unique_ptr<UpdateChecker> UpdateChecker::Create(
    const scoped_refptr<Configurator>& config,
    PersistedData* persistent) {
  return std::make_unique<UpdateCheckerImpl>(config, persistent);
}

}  // namespace update_client
