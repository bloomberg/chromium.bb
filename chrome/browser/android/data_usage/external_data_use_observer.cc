// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/external_data_use_observer.h"

#include "base/metrics/field_trial.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/data_usage/data_use_tab_model.h"
#include "chrome/browser/android/data_usage/external_data_use_observer_bridge.h"
#include "chrome/browser/android/data_usage/external_data_use_reporter.h"
#include "components/data_usage/core/data_use.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

namespace android {

namespace {

// Default duration after which matching rules are periodically fetched. May be
// overridden by the field trial.
const int kDefaultFetchMatchingRulesDurationSeconds = 60 * 15;  // 15 minutes.

// Populates various parameters from the values specified in the field trial.
int32_t GetFetchMatchingRulesDurationSeconds() {
  int32_t duration_seconds = -1;
  const std::string variation_value = variations::GetVariationParamValue(
      chrome::android::ExternalDataUseObserver::
          kExternalDataUseObserverFieldTrial,
      "fetch_matching_rules_duration_seconds");
  if (!variation_value.empty() &&
      base::StringToInt(variation_value, &duration_seconds)) {
    DCHECK_LE(0, duration_seconds);
    return duration_seconds;
  }
  return kDefaultFetchMatchingRulesDurationSeconds;
}

}  // namespace

// static
const char ExternalDataUseObserver::kExternalDataUseObserverFieldTrial[] =
    "ExternalDataUseObserver";

ExternalDataUseObserver::ExternalDataUseObserver(
    data_usage::DataUseAggregator* data_use_aggregator,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
    : data_use_aggregator_(data_use_aggregator),
      external_data_use_observer_bridge_(new ExternalDataUseObserverBridge()),
      data_use_tab_model_(new DataUseTabModel()),
      external_data_use_reporter_(
          new ExternalDataUseReporter(kExternalDataUseObserverFieldTrial,
                                      data_use_tab_model_,
                                      external_data_use_observer_bridge_)),
      ui_task_runner_(ui_task_runner),
      last_matching_rules_fetch_time_(base::TimeTicks::Now()),
      fetch_matching_rules_duration_(
          base::TimeDelta::FromSeconds(GetFetchMatchingRulesDurationSeconds())),
      registered_as_data_use_observer_(false),
      weak_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(data_use_aggregator_);
  DCHECK(io_task_runner);
  DCHECK(ui_task_runner_);

  // Initialize the ExternalDataUseReporter object. It is okay to use
  // base::Unretained here since |external_data_use_reporter_| is owned by
  // |this|, and is destroyed on UI thread when |this| is destroyed.
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ExternalDataUseReporter::InitOnUIThread,
                            base::Unretained(external_data_use_reporter_)));

  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&DataUseTabModel::InitOnUIThread,
                                       base::Unretained(data_use_tab_model_),
                                       external_data_use_observer_bridge_));

  // Initialize the ExternalDataUseObserverBridge object. It is okay to use
  // base::Unretained here since |external_data_use_observer_bridge_| is owned
  // by |this|, and is destroyed on UI thread when |this| is destroyed.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseObserverBridge::Init,
                 base::Unretained(external_data_use_observer_bridge_),
                 io_task_runner, GetWeakPtr(), data_use_tab_model_));
}

ExternalDataUseObserver::~ExternalDataUseObserver() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (registered_as_data_use_observer_)
    data_use_aggregator_->RemoveObserver(this);

  // Delete |external_data_use_reporter_| on the UI thread.
  // |external_data_use_reporter_| should be deleted before
  // |data_use_tab_model_| and |external_data_use_observer_bridge_|.
  if (!ui_task_runner_->DeleteSoon(FROM_HERE, external_data_use_reporter_)) {
    NOTIMPLEMENTED() << " ExternalDataUseReporter was not deleted successfully";
  }

  // Delete |data_use_tab_model_| on the UI thread. |data_use_tab_model_| should
  // be deleted before |external_data_use_observer_bridge_|.
  if (!ui_task_runner_->DeleteSoon(FROM_HERE, data_use_tab_model_)) {
    NOTIMPLEMENTED() << " DataUseTabModel was not deleted successfully";
  }

  // Delete |external_data_use_observer_bridge_| on the UI thread.
  if (!ui_task_runner_->DeleteSoon(FROM_HERE,
                                   external_data_use_observer_bridge_)) {
    NOTIMPLEMENTED()
        << " ExternalDataUseObserverBridge was not deleted successfully";
  }
}

void ExternalDataUseObserver::OnDataUse(const data_usage::DataUse& data_use) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(registered_as_data_use_observer_);

  const base::TimeTicks now_ticks = base::TimeTicks::Now();

  // If the time when the matching rules were last fetched is more than
  // |fetch_matching_rules_duration_|, fetch them again.
  if (now_ticks - last_matching_rules_fetch_time_ >=
      fetch_matching_rules_duration_) {
    FetchMatchingRules();
  }

  // It is okay to use base::Unretained here since |external_data_use_reporter_|
  // is owned by |this|, and is destroyed on UI thread when |this| is destroyed.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseReporter::OnDataUse,
                 base::Unretained(external_data_use_reporter_), data_use));
}

void ExternalDataUseObserver::OnReportDataUseDone(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // It is okay to use base::Unretained here since |external_data_use_reporter_|
  // is owned by |this|, and is destroyed on UI thread when |this| is destroyed.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseReporter::OnReportDataUseDone,
                 base::Unretained(external_data_use_reporter_), success));
}

void ExternalDataUseObserver::ShouldRegisterAsDataUseObserver(
    bool should_register) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (registered_as_data_use_observer_ == should_register)
    return;

  if (!registered_as_data_use_observer_ && should_register)
    data_use_aggregator_->AddObserver(this);

  if (registered_as_data_use_observer_ && !should_register)
    data_use_aggregator_->RemoveObserver(this);

  registered_as_data_use_observer_ = should_register;
}

void ExternalDataUseObserver::FetchMatchingRules() {
  DCHECK(thread_checker_.CalledOnValidThread());

  last_matching_rules_fetch_time_ = base::TimeTicks::Now();

  // It is okay to use base::Unretained here since
  // |external_data_use_observer_bridge_| is owned by |this|, and is destroyed
  // on UI thread when |this| is destroyed.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalDataUseObserverBridge::FetchMatchingRules,
                 base::Unretained(external_data_use_observer_bridge_)));
}

base::WeakPtr<ExternalDataUseObserver> ExternalDataUseObserver::GetWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

DataUseTabModel* ExternalDataUseObserver::GetDataUseTabModel() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return data_use_tab_model_;
}

}  // namespace android

}  // namespace chrome
