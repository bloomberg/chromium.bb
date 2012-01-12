// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DNS_DNS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DNS_DNS_API_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/io_thread.h"
#include "net/base/address_list.h"
#include "net/base/capturing_net_log.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver.h"

class IOThread;

namespace extensions {

extern const char kAddressKey[];
extern const char kResultCodeKey[];

class DNSResolveFunction : public AsyncExtensionFunction {
 public:
  DNSResolveFunction();
  virtual ~DNSResolveFunction();

  virtual bool RunImpl() OVERRIDE;

  // See below. Caller retains ownership.
  static void set_host_resolver_for_testing(
      net::HostResolver* host_resolver_for_testing);

 protected:
  void WorkOnIOThread();
  void RespondOnUIThread();

 private:
  std::string hostname_;

  bool response_;  // The value sent in SendResponse().

  // This instance is widely available through BrowserProcess, but we need to
  // acquire it on the UI thread and then use it on the IO thread, so we keep a
  // plain pointer to it here as we move from thread to thread.
  IOThread* io_thread_;

  // If null, then we'll use io_thread_ to obtain the real HostResolver. We use
  // a plain pointer for to be consistent with the ownership model of the real
  // one.
  static net::HostResolver* host_resolver_for_testing;

  scoped_ptr<net::CapturingBoundNetLog> capturing_bound_net_log_;
  scoped_ptr<net::HostResolver::RequestHandle> request_handle_;
  scoped_ptr<net::AddressList> addresses_;

  void OnLookupFinished(int result);

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.dns.resolve")
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DNS_DNS_API_H_
