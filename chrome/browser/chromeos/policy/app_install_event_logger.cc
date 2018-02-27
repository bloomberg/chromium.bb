// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_logger.h"

#include <algorithm>
#include <iterator>

#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_prefs.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr int kNonComplianceReasonAppNotInstalled = 5;

std::set<std::string> GetRequestedPackagesFromArcPolicy(
    const std::string& arc_policy) {
  std::unique_ptr<base::Value> dict = base::JSONReader::Read(arc_policy);
  if (!dict || !dict->is_dict()) {
    return {};
  }

  const base::Value* const packages =
      dict->FindKeyOfType("applications", base::Value::Type::LIST);
  if (!packages) {
    return {};
  }

  std::set<std::string> requested_packages;
  for (const auto& package : packages->GetList()) {
    if (!package.is_dict()) {
      continue;
    }
    const base::Value* const install_type =
        package.FindKeyOfType("installType", base::Value::Type::STRING);
    if (!install_type || install_type->GetString() != "REQUIRED") {
      continue;
    }
    const base::Value* const package_name =
        package.FindKeyOfType("packageName", base::Value::Type::STRING);
    if (!package_name || package_name->GetString().empty()) {
      continue;
    }
    requested_packages.insert(package_name->GetString());
  }
  return requested_packages;
}

std::set<std::string> GetRequestedPackagesFromPolicy(
    const policy::PolicyMap& policy) {
  const base::Value* const arc_enabled = policy.GetValue(key::kArcEnabled);
  if (!arc_enabled || !arc_enabled->is_bool() || !arc_enabled->GetBool()) {
    return {};
  }

  const base::Value* const arc_policy = policy.GetValue(key::kArcPolicy);
  if (!arc_policy || !arc_policy->is_string()) {
    return {};
  }

  return GetRequestedPackagesFromArcPolicy(arc_policy->GetString());
}

// Return all elements that are members of |first| but not |second|.
std::set<std::string> GetDifference(const std::set<std::string>& first,
                                    const std::set<std::string>& second) {
  std::set<std::string> difference;
  std::set_difference(first.begin(), first.end(), second.begin(), second.end(),
                      std::inserter(difference, difference.end()));
  return difference;
}

void EnsureTimestampSet(em::AppInstallReportLogEvent* event) {
  if (!event->has_timestamp()) {
    event->set_timestamp(
        (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds());
  }
}

em::AppInstallReportLogEvent CreateEvent(
    em::AppInstallReportLogEvent::EventType type) {
  em::AppInstallReportLogEvent event;
  EnsureTimestampSet(&event);
  event.set_event_type(type);
  return event;
}

}  // namespace

AppInstallEventLogger::AppInstallEventLogger(Delegate* delegate,
                                             Profile* profile)
    : delegate_(delegate), profile_(profile) {
  if (!arc::IsArcAllowedForProfile(profile_)) {
    delegate_->Add(GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending),
                   CreateEvent(em::AppInstallReportLogEvent::CANCELED));
    Clear(profile_);
    return;
  }

  policy::PolicyService* const policy_service =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile_)
          ->policy_service();
  EvaluatePolicy(policy_service->GetPolicies(policy::PolicyNamespace(
                     policy::POLICY_DOMAIN_CHROME, std::string())),
                 true /* initial */);

  observing_ = true;
  arc::ArcPolicyBridge* bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);
  bridge->AddObserver(this);
  policy_service->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
}

AppInstallEventLogger::~AppInstallEventLogger() {
  if (observing_) {
    arc::ArcPolicyBridge::GetForBrowserContext(profile_)->RemoveObserver(this);
    policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile_)
        ->policy_service()
        ->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  }
}

// static
void AppInstallEventLogger::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(arc::prefs::kArcPushInstallAppsRequested);
  registry->RegisterListPref(arc::prefs::kArcPushInstallAppsPending);
}

// static
void AppInstallEventLogger::Clear(Profile* profile) {
  profile->GetPrefs()->ClearPref(arc::prefs::kArcPushInstallAppsRequested);
  profile->GetPrefs()->ClearPref(arc::prefs::kArcPushInstallAppsPending);
}

void AppInstallEventLogger::AddForAllPackages(
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  EnsureTimestampSet(event.get());
  delegate_->Add(GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending),
                 *event);
}

void AppInstallEventLogger::Add(
    const std::string& package,
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  EnsureTimestampSet(event.get());
  delegate_->Add({package}, *event);
}

void AppInstallEventLogger::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                            const policy::PolicyMap& previous,
                                            const policy::PolicyMap& current) {
  EvaluatePolicy(current, false /* initial */);
}

void AppInstallEventLogger::OnPolicySent(const std::string& policy) {
  requested_in_arc_ = GetRequestedPackagesFromArcPolicy(policy);
}

void AppInstallEventLogger::OnComplianceReportReceived(
    const base::Value* compliance_report) {
  const base::Value* const details = compliance_report->FindKeyOfType(
      "nonComplianceDetails", base::Value::Type::LIST);
  if (!details) {
    return;
  }

  const std::set<std::string> previous_pending =
      GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending);

  std::set<std::string> pending_in_arc;
  for (const auto& detail : details->GetList()) {
    const base::Value* const reason =
        detail.FindKeyOfType("nonComplianceReason", base::Value::Type::INTEGER);
    if (!reason || reason->GetInt() != kNonComplianceReasonAppNotInstalled) {
      continue;
    }
    const base::Value* const app_name =
        detail.FindKeyOfType("packageName", base::Value::Type::STRING);
    if (!app_name || app_name->GetString().empty()) {
      continue;
    }
    pending_in_arc.insert(app_name->GetString());
  }
  const std::set<std::string> current_pending = GetDifference(
      previous_pending, GetDifference(requested_in_arc_, pending_in_arc));
  const std::set<std::string> removed =
      GetDifference(previous_pending, current_pending);
  // TODO(bartfab): Add SystemState.
  delegate_->Add(removed, CreateEvent(em::AppInstallReportLogEvent::SUCCESS));

  if (removed.empty()) {
    return;
  }

  SetPref(arc::prefs::kArcPushInstallAppsPending, current_pending);

  if (!current_pending.empty()) {
    UpdateCollector({} /* added */, removed);
  } else {
    StopCollector();
  }
}

std::set<std::string> AppInstallEventLogger::GetPackagesFromPref(
    const std::string& pref_name) const {
  std::set<std::string> packages;
  for (const auto& package :
       profile_->GetPrefs()->GetList(pref_name)->GetList()) {
    if (!package.is_string()) {
      continue;
    }
    packages.insert(package.GetString());
  }
  return packages;
}

void AppInstallEventLogger::SetPref(const std::string& pref_name,
                                    const std::set<std::string>& packages) {
  base::Value value(base::Value::Type::LIST);
  auto& list = value.GetList();
  for (const std::string& package : packages) {
    list.push_back(base::Value(package));
  }
  profile_->GetPrefs()->Set(pref_name, value);
}

void AppInstallEventLogger::UpdateCollector(
    const std::set<std::string>& added,
    const std::set<std::string>& removed) {
  if (!log_collector_) {
    log_collector_ =
        std::make_unique<AppInstallEventLogCollector>(this, profile_, added);
  } else {
    log_collector_->OnPendingPackagesChanged(added, removed);
  }
}

void AppInstallEventLogger::StopCollector() {
  log_collector_.reset();
}

void AppInstallEventLogger::EvaluatePolicy(const policy::PolicyMap& policy,
                                           bool initial) {
  const std::set<std::string> previous_requested =
      GetPackagesFromPref(arc::prefs::kArcPushInstallAppsRequested);
  const std::set<std::string> previous_pending =
      GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending);

  const std::set<std::string> current_requested =
      GetRequestedPackagesFromPolicy(policy);

  const std::set<std::string> added =
      GetDifference(current_requested, previous_requested);
  const std::set<std::string> removed =
      GetDifference(previous_pending, current_requested);
  // TODO: Add SystemState.
  delegate_->Add(added,
                 CreateEvent(em::AppInstallReportLogEvent::SERVER_REQUEST));
  delegate_->Add(removed, CreateEvent(em::AppInstallReportLogEvent::CANCELED));

  const std::set<std::string> current_pending = GetDifference(
      current_requested, GetDifference(previous_requested, previous_pending));
  SetPref(arc::prefs::kArcPushInstallAppsRequested, current_requested);
  SetPref(arc::prefs::kArcPushInstallAppsPending, current_pending);

  if (!current_pending.empty()) {
    if (initial) {
      UpdateCollector(current_pending /* added */, {} /* removed */);
    } else {
      UpdateCollector(added, removed);
    }
  } else {
    StopCollector();
  }
}

}  // namespace policy
