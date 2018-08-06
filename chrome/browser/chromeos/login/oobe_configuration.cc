// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oobe_configuration.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/task/post_task.h"

namespace chromeos {

namespace {

std::unique_ptr<base::Value> LoadOOBEConfigurationFile(base::FilePath path) {
  std::string configuration_data;
  if (!base::ReadFileToString(path, &configuration_data)) {
    DLOG(WARNING) << "Can't read OOBE Configuration";
    return std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  }
  int error_code, row, col;
  std::string error_message;
  auto value = base::JSONReader::ReadAndReturnError(
      configuration_data, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS,
      &error_code, &error_message, &row, &col);
  if (!value || !value->is_dict()) {
    LOG(ERROR) << "Error parsing OOBE configuration: " << error_message;
    return std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  }
  return value;
}

}  // namespace

// static
OobeConfiguration* OobeConfiguration::instance = nullptr;

OobeConfiguration::OobeConfiguration()
    : configuration_(
          std::make_unique<base::Value>(base::Value::Type::DICTIONARY)),
      weak_factory_(this) {
  DCHECK(!OobeConfiguration::Get());
  OobeConfiguration::instance = this;
}

OobeConfiguration::~OobeConfiguration() {
  DCHECK_EQ(instance, this);
  OobeConfiguration::instance = nullptr;
}

// static
OobeConfiguration* OobeConfiguration::Get() {
  return OobeConfiguration::instance;
}

void OobeConfiguration::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void OobeConfiguration::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

const base::Value& OobeConfiguration::GetConfiguration() const {
  return *configuration_.get();
}

void OobeConfiguration::ResetConfiguration() {
  configuration_ = std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  for (auto& observer : observer_list_)
    observer.OnOobeConfigurationChanged();
}

void OobeConfiguration::OnConfigurationLoaded(
    std::unique_ptr<base::Value> configuration) {
  LOG(ERROR) << " DEBUG OUT *****************OnConfigurationLoaded";
  if (!configuration)
    return;

  configuration_ = std::move(configuration);
  for (auto& observer : observer_list_)
    observer.OnOobeConfigurationChanged();
}

void OobeConfiguration::LoadConfiguration(const base::FilePath& path) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&LoadOOBEConfigurationFile, path),
      base::BindOnce(&OobeConfiguration::OnConfigurationLoaded,
                     weak_factory_.GetWeakPtr()));
}

}  // namespace chromeos
