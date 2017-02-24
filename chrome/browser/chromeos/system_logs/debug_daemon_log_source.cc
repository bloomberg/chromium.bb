// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/debug_daemon_log_source.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

const char kNotAvailable[] = "<not available>";
const char kRoutesKeyName[] = "routes";
const char kNetworkStatusKeyName[] = "network-status";
const char kModemStatusKeyName[] = "modem-status";
const char kWiMaxStatusKeyName[] = "wimax-status";
const char kUserLogFileKeyName[] = "user_log_files";

namespace system_logs {

DebugDaemonLogSource::DebugDaemonLogSource(bool scrub)
    : SystemLogsSource("DebugDemon"),
      response_(new SystemLogsResponse()),
      num_pending_requests_(0),
      scrub_(scrub),
      weak_ptr_factory_(this) {}

DebugDaemonLogSource::~DebugDaemonLogSource() {}

void DebugDaemonLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());
  DCHECK(callback_.is_null());

  callback_ = callback;
  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();

  client->GetRoutes(true,   // Numeric
                    false,  // No IPv6
                    base::Bind(&DebugDaemonLogSource::OnGetRoutes,
                               weak_ptr_factory_.GetWeakPtr()));
  ++num_pending_requests_;
  client->GetNetworkStatus(base::Bind(&DebugDaemonLogSource::OnGetNetworkStatus,
                                      weak_ptr_factory_.GetWeakPtr()));
  ++num_pending_requests_;
  client->GetModemStatus(base::Bind(&DebugDaemonLogSource::OnGetModemStatus,
                                    weak_ptr_factory_.GetWeakPtr()));
  ++num_pending_requests_;
  client->GetWiMaxStatus(base::Bind(&DebugDaemonLogSource::OnGetWiMaxStatus,
                                    weak_ptr_factory_.GetWeakPtr()));
  ++num_pending_requests_;
  client->GetUserLogFiles(base::Bind(&DebugDaemonLogSource::OnGetUserLogFiles,
                                     weak_ptr_factory_.GetWeakPtr()));
  ++num_pending_requests_;

  if (scrub_) {
    client->GetScrubbedBigLogs(base::Bind(&DebugDaemonLogSource::OnGetLogs,
                                          weak_ptr_factory_.GetWeakPtr()));
  } else {
    client->GetAllLogs(base::Bind(&DebugDaemonLogSource::OnGetLogs,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
  ++num_pending_requests_;
}

void DebugDaemonLogSource::OnGetRoutes(bool succeeded,
                                       const std::vector<std::string>& routes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (succeeded)
    (*response_)[kRoutesKeyName] = base::JoinString(routes, "\n");
  else
    (*response_)[kRoutesKeyName] = kNotAvailable;
  RequestCompleted();
}

void DebugDaemonLogSource::OnGetNetworkStatus(bool succeeded,
                                              const std::string& status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (succeeded)
    (*response_)[kNetworkStatusKeyName] = status;
  else
    (*response_)[kNetworkStatusKeyName] = kNotAvailable;
  RequestCompleted();
}

void DebugDaemonLogSource::OnGetModemStatus(bool succeeded,
                                            const std::string& status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (succeeded)
    (*response_)[kModemStatusKeyName] = status;
  else
    (*response_)[kModemStatusKeyName] = kNotAvailable;
  RequestCompleted();
}

void DebugDaemonLogSource::OnGetWiMaxStatus(bool succeeded,
                                            const std::string& status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  (*response_)[kWiMaxStatusKeyName] = succeeded ? status : kNotAvailable;
  RequestCompleted();
}

void DebugDaemonLogSource::OnGetLogs(bool /* succeeded */,
                                     const KeyValueMap& logs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // We ignore 'succeeded' for this callback - we want to display as much of the
  // debug info as we can even if we failed partway through parsing, and if we
  // couldn't fetch any of it, none of the fields will even appear.
  response_->insert(logs.begin(), logs.end());
  RequestCompleted();
}

void DebugDaemonLogSource::OnGetUserLogFiles(
    bool succeeded,
    const KeyValueMap& user_log_files) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (succeeded) {
    SystemLogsResponse* response = new SystemLogsResponse;

    const user_manager::UserList& users =
        user_manager::UserManager::Get()->GetLoggedInUsers();
    std::vector<base::FilePath> profile_dirs;
    for (user_manager::UserList::const_iterator it = users.begin();
         it != users.end();
         ++it) {
      if ((*it)->username_hash().empty())
        continue;
      profile_dirs.push_back(
          chromeos::ProfileHelper::GetProfilePathByUserIdHash(
              (*it)->username_hash()));
    }

    base::PostTaskWithTraitsAndReply(
        FROM_HERE, base::TaskTraits().MayBlock().WithPriority(
                       base::TaskPriority::BACKGROUND),
        base::Bind(&DebugDaemonLogSource::ReadUserLogFiles, user_log_files,
                   profile_dirs, response),
        base::Bind(&DebugDaemonLogSource::MergeResponse,
                   weak_ptr_factory_.GetWeakPtr(), base::Owned(response)));
  } else {
    (*response_)[kUserLogFileKeyName] = kNotAvailable;
    RequestCompleted();
  }
}

// static
void DebugDaemonLogSource::ReadUserLogFiles(
    const KeyValueMap& user_log_files,
    const std::vector<base::FilePath>& profile_dirs,
    SystemLogsResponse* response) {
  for (size_t i = 0; i < profile_dirs.size(); ++i) {
    std::string profile_prefix = "Profile[" + base::UintToString(i) + "] ";
    for (KeyValueMap::const_iterator it = user_log_files.begin();
         it != user_log_files.end();
         ++it) {
      std::string key = it->first;
      std::string value;
      std::string filename = it->second;
      bool read_success = base::ReadFileToString(
          profile_dirs[i].Append(filename), &value);

      if (read_success && !value.empty())
        (*response)[profile_prefix + key] = value;
      else
        (*response)[profile_prefix + filename] = kNotAvailable;
    }
  }
}

void DebugDaemonLogSource::MergeResponse(SystemLogsResponse* response) {
  for (SystemLogsResponse::const_iterator it = response->begin();
       it != response->end(); ++it)
    response_->insert(*it);
  RequestCompleted();
}

void DebugDaemonLogSource::RequestCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback_.is_null());

  --num_pending_requests_;
  if (num_pending_requests_ > 0)
    return;
  callback_.Run(response_.get());
}

}  // namespace system_logs
