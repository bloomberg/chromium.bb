// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/automation_constants.h"

namespace automation {

// JSON value labels for proxy settings that are passed in via
// AutomationMsg_SetProxyConfig.
const char kJSONProxyAutoconfig[] = "proxy.autoconfig";
const char kJSONProxyNoProxy[] = "proxy.no_proxy";
const char kJSONProxyPacUrl[] = "proxy.pac_url";
const char kJSONProxyPacMandatory[] = "proxy.pac_mandatory";
const char kJSONProxyBypassList[] = "proxy.bypass_list";
const char kJSONProxyServer[] = "proxy.server";

// Named testing interface is used when you want to connect an
// AutomationProxy to an already-running browser instance.
const char kNamedInterfacePrefix[] = "NamedTestingInterface:";

const int kChromeDriverAutomationVersion = 1;

namespace {

// Returns the string equivalent of the given |ErrorCode|.
const char* DefaultMessageForErrorCode(ErrorCode code) {
  switch (code) {
    case kUnknownError:
      return "Unknown error";
    case kNoJavaScriptModalDialogOpen:
      return "No JavaScript modal dialog is open";
    case kBlockedByModalDialog:
      return "Command blocked by an open modal dialog";
    case kInvalidId:
      return "ID is invalid or does not refer to an existing object";
    default:
      return "<unknown>";
  }
}

}  // namespace

Error::Error() : code_(kUnknownError) { }

Error::Error(ErrorCode code)
    : code_(code),
      message_(DefaultMessageForErrorCode(code)) { }

Error::Error(const std::string& error_msg)
    : code_(kUnknownError), message_(error_msg) { }

Error::Error(ErrorCode code, const std::string& error_msg)
    : code_(code), message_(error_msg) { }

Error::~Error() { }

ErrorCode Error::code() const {
  return code_;
}

const std::string& Error::message() const {
  return message_;
}

}  // namespace automation
