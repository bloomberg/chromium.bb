// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/sysinfo_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/browser/memory_details.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {
namespace system {

// Fetches logs from the debug daemon over DBus. When all the logs have been
// fetched, forwards the results to the supplied Request. Used like:
//   DebugDaemonLogFetcher* fetcher = new DebugDaemonLogFetcher(request);
//   fetcher->Fetch();
// Note that you do not need to delete the fetcher; it will delete itself after
// Fetch() has forwarded the result to the request handler.
class DebugDaemonLogFetcher {
 public:
  typedef
      scoped_refptr<CancelableRequest<SysInfoProvider::FetchCompleteCallback> >
      Request;

  explicit DebugDaemonLogFetcher(Request request);
  ~DebugDaemonLogFetcher();

  // Fetches logs from the daemon over dbus. After the fetch is complete, the
  // results will be forwarded to the request supplied to the constructor and
  // this instance will free itself.
  void Fetch();
 private:
  // Callbacks
  void GotRoutes(bool succeeded, const std::vector<std::string>& routes);
  void GotNetworkStatus(bool succeeded, const std::string& status);
  void GotModemStatus(bool succeeded, const std::string& status);
  void GotLogs(bool succeeded, const std::map<std::string, std::string>& logs);
  void RequestCompleted();

  SysInfoResponse* response_;
  Request request_;
  int pending_requests_;
  base::WeakPtrFactory<DebugDaemonLogFetcher> weak_ptr_factory_;
};

DebugDaemonLogFetcher::DebugDaemonLogFetcher(Request request)
    : response_(new SysInfoResponse()), request_(request),
      weak_ptr_factory_(this) { }

DebugDaemonLogFetcher::~DebugDaemonLogFetcher() { }

void DebugDaemonLogFetcher::Fetch() {
  DebugDaemonClient* client = DBusThreadManager::Get()->GetDebugDaemonClient();
  pending_requests_ = 4;
  // Note that this use of base::Unretained(this) is safe, since |this| is
  // deleted in RequestCompleted if and only if all outstanding callbacks are
  // done.
  client->GetRoutes(true, false,
      base::Bind(&DebugDaemonLogFetcher::GotRoutes,
                 weak_ptr_factory_.GetWeakPtr()));
  client->GetNetworkStatus(
      base::Bind(&DebugDaemonLogFetcher::GotNetworkStatus,
                 weak_ptr_factory_.GetWeakPtr()));
  client->GetModemStatus(
      base::Bind(&DebugDaemonLogFetcher::GotModemStatus,
                 weak_ptr_factory_.GetWeakPtr()));
  client->GetAllLogs(
      base::Bind(&DebugDaemonLogFetcher::GotLogs,
                 weak_ptr_factory_.GetWeakPtr()));
}

const char *kNotAvailable = "<not available>";

void DebugDaemonLogFetcher::GotRoutes(bool succeeded,
                                      const std::vector<std::string>& routes) {
  if (succeeded)
    (*response_)["routes"] = JoinString(routes, '\n');
  else
    (*response_)["routes"] = kNotAvailable;
  RequestCompleted();
}

void DebugDaemonLogFetcher::GotNetworkStatus(bool succeeded,
                                             const std::string& status) {
  if (succeeded)
    (*response_)["network-status"] = status;
  else
    (*response_)["network-status"] = kNotAvailable;
  RequestCompleted();
}

void DebugDaemonLogFetcher::GotModemStatus(bool succeeded,
                                           const std::string& status) {
  if (succeeded)
    (*response_)["modem-status"] = status;
  else
    (*response_)["modem-status"] = kNotAvailable;
  RequestCompleted();
}

void DebugDaemonLogFetcher::GotLogs(bool /* succeeded */,
                                    const std::map<std::string,
                                                   std::string>& logs) {
  // We ignore 'succeeded' for this callback - we want to display as much of the
  // debug info as we can even if we failed partway through parsing, and if we
  // couldn't fetch any of it, none of the fields will even appear.
  for (std::map<std::string, std::string>::const_iterator it = logs.begin();
       it != logs.end(); ++it)
    (*response_)[it->first] = it->second;
  RequestCompleted();
}

void DebugDaemonLogFetcher::RequestCompleted() {
  if (--pending_requests_)
    return;
  request_->ForwardResult(response_);
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
}

class SysInfoProviderImpl : public SysInfoProvider {
 public:
  virtual Handle Fetch(CancelableRequestConsumerBase* consumer,
                       const FetchCompleteCallback& callback);
};

CancelableRequestProvider::Handle SysInfoProviderImpl::Fetch(
    CancelableRequestConsumerBase* consumer,
    const FetchCompleteCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<CancelableRequest<FetchCompleteCallback> >
      request(new CancelableRequest<FetchCompleteCallback>(callback));
  AddRequest(request, consumer);
  // Deleted by DebugDaemonLogFetcher::RequestCompleted()
  DebugDaemonLogFetcher* fetcher = new DebugDaemonLogFetcher(request);
  fetcher->Fetch();

  return request->handle();
}

SysInfoProvider* SysInfoProvider::Create() {
  return new SysInfoProviderImpl();
}

} // namespace system
} // namespace chromeos
