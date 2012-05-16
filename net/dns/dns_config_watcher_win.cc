// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_watcher.h"

#include <winsock2.h>

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "base/win/object_watcher.h"
#include "base/win/registry.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/dns_config_service_win.h"
#include "net/dns/file_path_watcher_wrapper.h"

namespace net {
namespace internal {

namespace {

// Watches a single registry key for changes.
class RegistryWatcher : public base::win::ObjectWatcher::Delegate,
                        public base::NonThreadSafe {
 public:
  typedef base::Callback<void(bool succeeded)> CallbackType;
  RegistryWatcher() {}

  bool Watch(const wchar_t* key, const CallbackType& callback) {
    DCHECK(CalledOnValidThread());
    DCHECK(!callback.is_null());
    DCHECK(callback_.is_null());
    callback_ = callback;
    if (key_.Open(HKEY_LOCAL_MACHINE, key, KEY_NOTIFY) != ERROR_SUCCESS)
      return false;
    if (key_.StartWatching() != ERROR_SUCCESS)
      return false;
    if (!watcher_.StartWatching(key_.watch_event(), this))
      return false;
    return true;
  }

  virtual void OnObjectSignaled(HANDLE object) OVERRIDE {
    DCHECK(CalledOnValidThread());
    bool succeeded = (key_.StartWatching() == ERROR_SUCCESS) &&
                      watcher_.StartWatching(key_.watch_event(), this);
    if (!succeeded) {
      if (key_.Valid()) {
        watcher_.StopWatching();
        key_.StopWatching();
        key_.Close();
      }
    }
    if (!callback_.is_null())
      callback_.Run(succeeded);
  }

 private:
  CallbackType callback_;
  base::win::RegKey key_;
  base::win::ObjectWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(RegistryWatcher);
};

}  // namespace

// Watches registry for changes. Setting up watches requires IO loop.
class DnsConfigWatcher::Core {
 public:
  Core() {}
  ~Core() {}

  bool Watch() {
    RegistryWatcher::CallbackType callback =
        base::Bind(&Core::OnRegistryChanged, base::Unretained(this));

    bool success = true;

    // The Tcpip key must be present.
    if (!tcpip_watcher_.Watch(kTcpipPath, callback)) {
      LOG(ERROR) << "DNS registry watch failed to start.";
      success = false;
    }

    // Watch for IPv6 nameservers.
    tcpip6_watcher_.Watch(kTcpip6Path, callback);

    // DNS suffix search list and devolution can be configured via group
    // policy which sets this registry key. If the key is missing, the policy
    // does not apply, and the DNS client uses Tcpip and Dnscache settings.
    // If a policy is installed, DnsConfigService will need to be restarted.
    // BUG=99509

    dnscache_watcher_.Watch(kDnscachePath, callback);
    policy_watcher_.Watch(kPolicyPath, callback);

    if (!hosts_watcher_.Watch(GetHostsPath(),
                              base::Bind(&Core::OnHostsChanged,
                                         base::Unretained(this)))) {
      LOG(ERROR) << "DNS hosts watch failed to start.";
      success = false;
    }
    return success;
  }

 private:
  void OnRegistryChanged(bool succeeded) {
    if (succeeded) {
      NetworkChangeNotifier::NotifyObserversOfDNSChange(
          NetworkChangeNotifier::CHANGE_DNS_SETTINGS);
    } else {
      LOG(ERROR) << "DNS config watch failed.";
      NetworkChangeNotifier::NotifyObserversOfDNSChange(
          NetworkChangeNotifier::CHANGE_DNS_WATCH_FAILED);
    }
  }

  void OnHostsChanged(bool succeeded) {
    if (succeeded) {
      NetworkChangeNotifier::NotifyObserversOfDNSChange(
          NetworkChangeNotifier::CHANGE_DNS_HOSTS);
    } else {
      LOG(ERROR) << "DNS hosts watch failed.";
      NetworkChangeNotifier::NotifyObserversOfDNSChange(
          NetworkChangeNotifier::CHANGE_DNS_WATCH_FAILED);
    }
  }

  RegistryWatcher tcpip_watcher_;
  RegistryWatcher tcpip6_watcher_;
  RegistryWatcher dnscache_watcher_;
  RegistryWatcher policy_watcher_;
  FilePathWatcherWrapper hosts_watcher_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

DnsConfigWatcher::DnsConfigWatcher() {}

DnsConfigWatcher::~DnsConfigWatcher() {}

void DnsConfigWatcher::Init() {
  core_.reset(new Core());
  if (core_->Watch()) {
    NetworkChangeNotifier::NotifyObserversOfDNSChange(
        NetworkChangeNotifier::CHANGE_DNS_WATCH_STARTED);
  }
  // TODO(szym): re-start watcher if that makes sense. http://crbug.com/116139
}

void DnsConfigWatcher::CleanUp() {
  core_.reset();
}

}  // namespace internal
}  // namespace net

