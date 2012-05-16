// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_watcher.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/file_path_watcher_wrapper.h"

#if defined(OS_MACOSX)
#include "net/dns/notify_watcher_mac.h"
#endif

namespace net {
namespace internal {

namespace {

const FilePath::CharType* kFilePathHosts = FILE_PATH_LITERAL("/etc/hosts");

#if defined(OS_MACOSX)
// From 10.7.3 configd-395.10/dnsinfo/dnsinfo.h
static const char* kDnsNotifyKey =
    "com.apple.system.SystemConfiguration.dns_configuration";

class ConfigWatcher {
 public:
  bool Watch(const base::Callback<void(bool succeeded)>& callback) {
    return watcher_.Watch(kDnsNotifyKey, callback);
  }
 private:
  NotifyWatcherMac watcher_;
};
#else

#ifndef _PATH_RESCONF  // Normally defined in <resolv.h>
#define _PATH_RESCONF "/etc/resolv.conf"
#endif

static const FilePath::CharType* kFilePathConfig =
    FILE_PATH_LITERAL(_PATH_RESCONF);

class ConfigWatcher {
 public:
  bool Watch(const base::Callback<void(bool succeeded)>& callback) {
    return watcher_.Watch(FilePath(kFilePathConfig), callback);
  }
 private:
  FilePathWatcherWrapper watcher_;
};
#endif

}  // namespace

class DnsConfigWatcher::Core {
 public:
  Core() {}
  ~Core() {}

  bool Watch() {
    bool success = true;
    if (!config_watcher_.Watch(base::Bind(&Core::OnConfigChanged,
                                          base::Unretained(this)))) {
      LOG(ERROR) << "DNS config watch failed to start.";
      success = false;
    }
    if (!hosts_watcher_.Watch(FilePath(kFilePathHosts),
                              base::Bind(&Core::OnHostsChanged,
                                         base::Unretained(this)))) {
      LOG(ERROR) << "DNS hosts watch failed to start.";
      success = false;
    }
    return success;
  }

 private:
  void OnConfigChanged(bool succeeded) {
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

  ConfigWatcher config_watcher_;
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
