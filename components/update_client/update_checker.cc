// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_checker.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/update_client.h"
#include "components/update_client/updater_state.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

namespace update_client {

namespace {

// Returns a sanitized version of the brand or an empty string otherwise.
std::string SanitizeBrand(const std::string& brand) {
  return IsValidBrand(brand) ? brand : std::string("");
}

// Filters invalid attributes from |installer_attributes|.
update_client::InstallerAttributes SanitizeInstallerAttributes(
    const update_client::InstallerAttributes& installer_attributes) {
  update_client::InstallerAttributes sanitized_attrs;
  for (const auto& attr : installer_attributes) {
    if (IsValidInstallerAttribute(attr))
      sanitized_attrs.insert(attr);
  }
  return sanitized_attrs;
}

// Returns true if at least one item requires network encryption.
bool IsEncryptionRequired(const IdToComponentPtrMap& components) {
  for (const auto& item : components) {
    const auto& component = item.second;
    if (component->crx_component().requires_network_encryption)
      return true;
  }
  return false;
}

// Builds an update check request for |components|. |additional_attributes| is
// serialized as part of the <request> element of the request to customize it
// with data that is not platform or component specific. For each |item|, a
// corresponding <app> element is created and inserted as a child node of
// the <request>.
//
// An app element looks like this:
//    <app appid="hnimpnehoodheedghdeeijklkeaacbdc"
//         version="0.1.2.3" installsource="ondemand">
//      <updatecheck/>
//      <packages>
//        <package fp="abcd"/>
//      </packages>
//    </app>
std::string BuildUpdateCheckRequest(
    const Configurator& config,
    const std::vector<std::string>& ids_checked,
    const IdToComponentPtrMap& components,
    PersistedData* metadata,
    const std::string& additional_attributes,
    bool enabled_component_updates,
    const std::unique_ptr<UpdaterState::Attributes>& updater_state_attributes) {
  const std::string brand(SanitizeBrand(config.GetBrand()));
  std::string app_elements;
  for (const auto& id : ids_checked) {
    DCHECK_EQ(1u, components.count(id));
    const Component& component = *components.at(id);

    const update_client::InstallerAttributes installer_attributes(
        SanitizeInstallerAttributes(
            component.crx_component().installer_attributes));
    std::string app("<app ");
    base::StringAppendF(&app, "appid=\"%s\" version=\"%s\"",
                        component.id().c_str(),
                        component.crx_component().version.GetString().c_str());
    if (!brand.empty())
      base::StringAppendF(&app, " brand=\"%s\"", brand.c_str());
    if (component.on_demand())
      base::StringAppendF(&app, " installsource=\"ondemand\"");
    for (const auto& attr : installer_attributes) {
      base::StringAppendF(&app, " %s=\"%s\"", attr.first.c_str(),
                          attr.second.c_str());
    }
    const std::string cohort = metadata->GetCohort(component.id());
    const std::string cohort_name = metadata->GetCohortName(component.id());
    const std::string cohort_hint = metadata->GetCohortHint(component.id());
    if (!cohort.empty())
      base::StringAppendF(&app, " cohort=\"%s\"", cohort.c_str());
    if (!cohort_name.empty())
      base::StringAppendF(&app, " cohortname=\"%s\"", cohort_name.c_str());
    if (!cohort_hint.empty())
      base::StringAppendF(&app, " cohorthint=\"%s\"", cohort_hint.c_str());
    base::StringAppendF(&app, ">");

    base::StringAppendF(&app, "<updatecheck");
    if (component.crx_component()
            .supports_group_policy_enable_component_updates &&
        !enabled_component_updates) {
      base::StringAppendF(&app, " updatedisabled=\"true\"");
    }
    base::StringAppendF(&app, "/>");

    base::StringAppendF(&app, "<ping rd=\"%d\" ping_freshness=\"%s\"/>",
                        metadata->GetDateLastRollCall(component.id()),
                        metadata->GetPingFreshness(component.id()).c_str());
    if (!component.crx_component().fingerprint.empty()) {
      base::StringAppendF(&app,
                          "<packages>"
                          "<package fp=\"%s\"/>"
                          "</packages>",
                          component.crx_component().fingerprint.c_str());
    }
    base::StringAppendF(&app, "</app>");
    app_elements.append(app);
    VLOG(1) << "Appending to update request: " << app;
  }

  // Include the updater state in the update check request.
  return BuildProtocolRequest(
      config.GetProdId(), config.GetBrowserVersion().GetString(),
      config.GetChannel(), config.GetLang(), config.GetOSLongName(),
      config.GetDownloadPreference(), app_elements, additional_attributes,
      updater_state_attributes);
}

class UpdateCheckerImpl : public UpdateChecker {
 public:
  UpdateCheckerImpl(const scoped_refptr<Configurator>& config,
                    PersistedData* metadata);
  ~UpdateCheckerImpl() override;

  // Overrides for UpdateChecker.
  bool CheckForUpdates(
      const std::vector<std::string>& ids_checked,
      const IdToComponentPtrMap& components,
      const std::string& additional_attributes,
      bool enabled_component_updates,
      const UpdateCheckCallback& update_check_callback) override;

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
                            const UpdateResponse::Results& results,
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

bool UpdateCheckerImpl::CheckForUpdates(
    const std::vector<std::string>& ids_checked,
    const IdToComponentPtrMap& components,
    const std::string& additional_attributes,
    bool enabled_component_updates,
    const UpdateCheckCallback& update_check_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ids_checked_ = ids_checked;
  update_check_callback_ = update_check_callback;

  return config_->GetSequencedTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&UpdateCheckerImpl::ReadUpdaterStateAttributes,
                 base::Unretained(this)),
      base::Bind(&UpdateCheckerImpl::CheckForUpdatesHelper,
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

  request_sender_ = base::MakeUnique<RequestSender>(config_);
  request_sender_->Send(
      config_->EnabledCupSigning(),
      BuildUpdateCheckRequest(*config_, ids_checked_, components, metadata_,
                              additional_attributes, enabled_component_updates,
                              updater_state_attributes_),
      urls,
      base::Bind(&UpdateCheckerImpl::OnRequestSenderComplete,
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

  UpdateResponse update_response;
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
    const UpdateResponse::Results& results,
    int retry_after_sec) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const int daynum = results.daystart_elapsed_days;
  if (daynum != UpdateResponse::kNoDaystart)
    metadata_->SetDateLastRollCall(ids_checked_, daynum);
  for (const auto& result : results.list) {
    auto entry = result.cohort_attrs.find(UpdateResponse::Result::kCohort);
    if (entry != result.cohort_attrs.end())
      metadata_->SetCohort(result.extension_id, entry->second);
    entry = result.cohort_attrs.find(UpdateResponse::Result::kCohortName);
    if (entry != result.cohort_attrs.end())
      metadata_->SetCohortName(result.extension_id, entry->second);
    entry = result.cohort_attrs.find(UpdateResponse::Result::kCohortHint);
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
      FROM_HERE, base::Bind(update_check_callback_, 0, retry_after_sec));
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
      FROM_HERE, base::Bind(update_check_callback_, error, retry_after_sec));
}

}  // namespace

std::unique_ptr<UpdateChecker> UpdateChecker::Create(
    const scoped_refptr<Configurator>& config,
    PersistedData* persistent) {
  return base::MakeUnique<UpdateCheckerImpl>(config, persistent);
}

}  // namespace update_client
