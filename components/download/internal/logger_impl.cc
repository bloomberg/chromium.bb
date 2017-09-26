// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/logger_impl.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/download/internal/log_source.h"

namespace download {
namespace {

std::string ControllerStateToString(Controller::State state) {
  switch (state) {
    case Controller::State::CREATED:
      return "CREATED";
    case Controller::State::INITIALIZING:
      return "INITIALIZING";
    case Controller::State::READY:
      return "READY";
    case Controller::State::RECOVERING:
      return "RECOVERING";
    case Controller::State::UNAVAILABLE:  // Intentional fallthrough.
    default:
      return "UNAVAILABLE";
  }
}

std::string OptBoolToString(base::Optional<bool> value) {
  if (value.has_value())
    return value.value() ? "OK" : "BAD";

  return "UNKNOWN";
}

}  // namespace

LoggerImpl::LoggerImpl() : log_source_(nullptr) {}
LoggerImpl::~LoggerImpl() = default;

void LoggerImpl::SetLogSource(LogSource* log_source) {
  log_source_ = log_source;
}

void LoggerImpl::AddObserver(Observer* observer) {
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void LoggerImpl::RemoveObserver(Observer* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

base::Value LoggerImpl::GetServiceStatus() {
  base::DictionaryValue service_status;

  if (!log_source_)
    return std::move(service_status);

  Controller::State state = log_source_->GetControllerState();
  const StartupStatus& status = log_source_->GetStartupStatus();

  service_status.SetString("serviceState", ControllerStateToString(state));
  service_status.SetString("modelStatus", OptBoolToString(status.model_ok));
  service_status.SetString("driverStatus", OptBoolToString(status.driver_ok));
  service_status.SetString("fileMonitorStatus",
                           OptBoolToString(status.file_monitor_ok));

  return std::move(service_status);
}

void LoggerImpl::OnServiceStatusChanged() {
  if (!observers_.might_have_observers())
    return;

  base::Value service_status = GetServiceStatus();

  for (auto& observer : observers_)
    observer.OnServiceStatusChanged(service_status);
}

}  // namespace download
