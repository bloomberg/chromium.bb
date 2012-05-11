// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/broker_process_dispatcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/utf_string_conversions.h"
#include "content/common/child_process.h"
#include "ppapi/c/private/ppp_flash_browser_operations.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace {

// How long we wait before releasing the broker process.
const int kBrokerReleaseTimeSeconds = 30;

}  // namespace

BrokerProcessDispatcher::BrokerProcessDispatcher(
    PP_GetInterface_Func get_plugin_interface,
    PP_ConnectInstance_Func connect_instance)
    : ppapi::proxy::BrokerSideDispatcher(connect_instance),
      get_plugin_interface_(get_plugin_interface) {
  ChildProcess::current()->AddRefProcess();
}

BrokerProcessDispatcher::~BrokerProcessDispatcher() {
  DVLOG(1) << "BrokerProcessDispatcher::~BrokerProcessDispatcher()";
  // Don't free the process right away. This timer allows the child process
  // to be re-used if the user rapidly goes to a new page that requires this
  // plugin. This is the case for common plugins where they may be used on a
  // source and destination page of a navigation. We don't want to tear down
  // and re-start processes each time in these cases.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ChildProcess::ReleaseProcess,
                 base::Unretained(ChildProcess::current())),
      base::TimeDelta::FromSeconds(kBrokerReleaseTimeSeconds));
}

bool BrokerProcessDispatcher::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(BrokerProcessDispatcher, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_ClearSiteData, OnMsgClearSiteData)
    IPC_MESSAGE_UNHANDLED(return BrokerSideDispatcher::OnMessageReceived(msg))
  IPC_END_MESSAGE_MAP()
  return true;
}

void BrokerProcessDispatcher::OnMsgClearSiteData(
    const FilePath& plugin_data_path,
    const std::string& site,
    uint64 flags,
    uint64 max_age) {
  Send(new PpapiHostMsg_ClearSiteDataResult(
      ClearSiteData(plugin_data_path, site, flags, max_age)));
}

bool BrokerProcessDispatcher::ClearSiteData(const FilePath& plugin_data_path,
                                            const std::string& site,
                                            uint64 flags,
                                            uint64 max_age) {
  if (!get_plugin_interface_)
    return false;
  const PPP_Flash_BrowserOperations_1_0* browser_interface =
      static_cast<const PPP_Flash_BrowserOperations_1_0*>(
          get_plugin_interface_(PPP_FLASH_BROWSEROPERATIONS_INTERFACE_1_0));
  if (!browser_interface)
    return false;

  // The string is always 8-bit, convert on Windows.
#if defined(OS_WIN)
  std::string data_str = WideToUTF8(plugin_data_path.value());
#else
  std::string data_str = plugin_data_path.value();
#endif

  browser_interface->ClearSiteData(data_str.c_str(),
                                   site.empty() ? NULL : site.c_str(),
                                   flags, max_age);
  return true;
}

