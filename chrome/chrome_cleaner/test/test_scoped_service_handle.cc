// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_scoped_service_handle.h"

#include <windows.h>

#include <vector>

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process_iterator.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/chrome_cleaner/os/scoped_service_handle.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/test/test_executables.h"

namespace chrome_cleaner {

namespace {

// The sleep time in ms between each poll attempt to get information about a
// service.
constexpr unsigned int kServiceQueryWaitTimeMs = 250;

// The number of attempts to contact a service.
constexpr int kServiceQueryRetry = 5;

}  // namespace

TestScopedServiceHandle::~TestScopedServiceHandle() {
  StopAndDelete();
}

bool TestScopedServiceHandle::InstallService() {
  // Construct the full-path of the test service.
  base::FilePath module_path;
  if (!base::PathService::Get(base::DIR_EXE, &module_path)) {
    PLOG(ERROR) << "Cannot retrieve module name.";
    return false;
  }
  module_path = module_path.Append(kTestServiceExecutableName);

  DCHECK(!service_manager_.IsValid());
  DCHECK(!service_.IsValid());
  DCHECK(service_name_.empty());

  // Find an unused name for this service.
  base::string16 service_name = RandomUnusedServiceNameForTesting();

  // Get a handle to the service manager.
  service_manager_.Set(
      ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!service_manager_.IsValid()) {
    PLOG(ERROR) << "Cannot open service manager.";
    return false;
  }

  const base::string16 service_desc =
      base::StrCat({service_name, L" - Chrome Cleanup Tool (test)"});

  service_.Set(::CreateServiceW(
      service_manager_.Get(), service_name.c_str(), service_desc.c_str(),
      SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
      SERVICE_ERROR_NORMAL, module_path.value().c_str(), nullptr, nullptr,
      nullptr, nullptr, nullptr));
  if (!service_.IsValid()) {
    // Unable to create the service.
    PLOG(ERROR) << "Cannot create service '" << service_name << "'.";
    return false;
  }
  LOG(INFO) << "Created test service '" << service_name << "'.";
  service_name_ = service_name;
  return true;
}

bool TestScopedServiceHandle::StartService() {
  if (!::StartService(service_.Get(), 0, nullptr))
    return false;
  SERVICE_STATUS service_status = {};
  for (int iteration = 0; iteration < kServiceQueryRetry; ++iteration) {
    if (!::QueryServiceStatus(service_.Get(), &service_status))
      return false;
    if (service_status.dwCurrentState == SERVICE_RUNNING)
      return true;
    ::Sleep(kServiceQueryWaitTimeMs);
  }
  return false;
}

bool TestScopedServiceHandle::StopAndDelete() {
  Close();
  if (service_name_.empty())
    return false;
  bool result = StopAndDeleteService(service_name_.c_str());
  service_name_.clear();
  return result;
}

void TestScopedServiceHandle::Close() {
  service_.Close();
  service_manager_.Close();
}

bool TestScopedServiceHandle::StopAndDeleteService(
    const base::string16& service_name) {
  return StopService(service_name.c_str()) &&
         DeleteService(service_name.c_str()) &&
         WaitForServiceDeleted(service_name.c_str());
}

base::string16 RandomUnusedServiceNameForTesting() {
  base::string16 service_name;
  do {
    service_name =
        base::UTF8ToUTF16(base::UnguessableToken::Create().ToString());
  } while (DoesServiceExist(service_name.c_str()));
  return service_name;
}

bool EnsureNoTestServicesRunning() {
  // Get the pid's of all processes running the test service executable.
  base::ProcessIterator::ProcessEntries processes =
      base::NamedProcessIterator(kTestServiceExecutableName, nullptr)
          .Snapshot();
  if (processes.empty())
    return true;

  std::vector<base::ProcessId> process_ids;
  process_ids.reserve(processes.size());
  for (const auto& process : processes) {
    process_ids.push_back(process.pid());
  }

  // Iterate through all installed services. Stop and delete all those with
  // pid's in the list.
  ScopedScHandle service_manager(::OpenSCManager(
      nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE));

  std::vector<ServiceStatus> services;
  if (!EnumerateServices(service_manager, SERVICE_WIN32_OWN_PROCESS,
                         SERVICE_STATE_ALL, &services)) {
    return false;
  }
  std::vector<base::string16> stopped_service_names;
  for (const ServiceStatus& service : services) {
    base::string16 service_name = service.service_name;
    base::ProcessId pid = service.service_status_process.dwProcessId;
    if (base::ContainsValue(process_ids, pid)) {
      if (!StopService(service_name.c_str())) {
        LOG(ERROR) << "Could not stop service " << service_name;
        return false;
      }
      stopped_service_names.push_back(service_name);
    }
  }

  // Now all services running the test executable should be stopping, and can be
  // deleted.
  if (!WaitForProcessesStopped(kTestServiceExecutableName)) {
    LOG(ERROR) << "Not all " << kTestServiceExecutableName
               << " processes stopped";
    return false;
  }
  // Issue an async DeleteService request for each service in parallel, then
  // wait for all of them to finish deleting.
  for (const base::string16& service_name : stopped_service_names) {
    if (!DeleteService(service_name.c_str())) {
      LOG(ERROR) << "Could not delete service " << service_name;
      return false;
    }
  }
  for (const base::string16& service_name : stopped_service_names) {
    if (!WaitForServiceDeleted(service_name.c_str())) {
      LOG(ERROR) << "Did not finish deleting service " << service_name;
      return false;
    }
  }
  return true;
}

}  // namespace chrome_cleaner
