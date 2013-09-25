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

WebrtcLoggingPrivateStartFunction::WebrtcLoggingPrivateStartFunction() {}

WebrtcLoggingPrivateStartFunction::~WebrtcLoggingPrivateStartFunction() {}

bool WebrtcLoggingPrivateStartFunction::RunImpl() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

void WebrtcLoggingPrivateStartFunction::StartCallback(bool success) {}

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

void WebrtcLoggingPrivateStopFunction::StopCallback(bool success) {}

WebrtcLoggingPrivateUploadFunction::WebrtcLoggingPrivateUploadFunction() {}

WebrtcLoggingPrivateUploadFunction::~WebrtcLoggingPrivateUploadFunction() {}

bool WebrtcLoggingPrivateUploadFunction::RunImpl() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

void WebrtcLoggingPrivateUploadFunction::UploadCallback(
    bool success, std::string report_id) {
}

WebrtcLoggingPrivateDiscardFunction::WebrtcLoggingPrivateDiscardFunction() {}

WebrtcLoggingPrivateDiscardFunction::~WebrtcLoggingPrivateDiscardFunction() {}

bool WebrtcLoggingPrivateDiscardFunction::RunImpl() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

void WebrtcLoggingPrivateDiscardFunction::DiscardCallback(bool success) {}

}  // namespace extensions
