// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Stub implementation used when WebRTC is not enabled.

#include "chrome/browser/extensions/api/webrtc_logging_private/webrtc_logging_private_api.h"

namespace extensions {

namespace {

const char kErrorNotSupported[] = "Not supported";

}  // namespace

WebrtcLoggingPrivateSetMetaDataFunction::
WebrtcLoggingPrivateSetMetaDataFunction() {}

WebrtcLoggingPrivateSetMetaDataFunction::
~WebrtcLoggingPrivateSetMetaDataFunction() {}

bool WebrtcLoggingPrivateSetMetaDataFunction::RunImpl() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

void WebrtcLoggingPrivateSetMetaDataFunction::SetMetaDataCallback(
    bool success, const std::string& error_message) {}

WebrtcLoggingPrivateStartFunction::WebrtcLoggingPrivateStartFunction() {}

WebrtcLoggingPrivateStartFunction::~WebrtcLoggingPrivateStartFunction() {}

bool WebrtcLoggingPrivateStartFunction::RunImpl() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

void WebrtcLoggingPrivateStartFunction::StartCallback(
    bool success, const std::string& error_message) {}

WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::
WebrtcLoggingPrivateSetUploadOnRenderCloseFunction() {}

WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::
~WebrtcLoggingPrivateSetUploadOnRenderCloseFunction() {}

bool WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::RunImpl() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

WebrtcLoggingPrivateStopFunction::WebrtcLoggingPrivateStopFunction() {}

WebrtcLoggingPrivateStopFunction::~WebrtcLoggingPrivateStopFunction() {}

bool WebrtcLoggingPrivateStopFunction::RunImpl() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

void WebrtcLoggingPrivateStopFunction::StopCallback(
    bool success, const std::string& error_message) {}

WebrtcLoggingPrivateUploadFunction::WebrtcLoggingPrivateUploadFunction() {}

WebrtcLoggingPrivateUploadFunction::~WebrtcLoggingPrivateUploadFunction() {}

bool WebrtcLoggingPrivateUploadFunction::RunImpl() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

void WebrtcLoggingPrivateUploadFunction::UploadCallback(
    bool success, const std::string& report_id,
    const std::string& error_message) {
}

WebrtcLoggingPrivateDiscardFunction::WebrtcLoggingPrivateDiscardFunction() {}

WebrtcLoggingPrivateDiscardFunction::~WebrtcLoggingPrivateDiscardFunction() {}

bool WebrtcLoggingPrivateDiscardFunction::RunImpl() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

void WebrtcLoggingPrivateDiscardFunction::DiscardCallback(
    bool success, const std::string& error_message) {}

}  // namespace extensions
