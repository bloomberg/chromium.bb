// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webrtc_logging_private/webrtc_logging_private_api.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

namespace SetMetaData = api::webrtc_logging_private::SetMetaData;
namespace SetUploadOnRenderClose =
    api::webrtc_logging_private::SetUploadOnRenderClose;

WebrtcLoggingPrivateSetMetaDataFunction::
WebrtcLoggingPrivateSetMetaDataFunction() {}

WebrtcLoggingPrivateSetMetaDataFunction::
~WebrtcLoggingPrivateSetMetaDataFunction() {}

bool WebrtcLoggingPrivateSetMetaDataFunction::RunImpl() {
  scoped_ptr<SetMetaData::Params> params(SetMetaData::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
  return false;
}

WebrtcLoggingPrivateStartFunction::WebrtcLoggingPrivateStartFunction() {}

WebrtcLoggingPrivateStartFunction::~WebrtcLoggingPrivateStartFunction() {}

bool WebrtcLoggingPrivateStartFunction::RunImpl() {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
  return false;
}

void WebrtcLoggingPrivateStartFunction::StartCallback(bool success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // TODO(grunell): Implement set lastError.
  NOTIMPLEMENTED();
  SendResponse(success);
}

WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::
WebrtcLoggingPrivateSetUploadOnRenderCloseFunction() {}

WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::
~WebrtcLoggingPrivateSetUploadOnRenderCloseFunction() {}

bool WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::RunImpl() {
  scoped_ptr<SetUploadOnRenderClose::Params> params(
      SetUploadOnRenderClose::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
  return false;
}

WebrtcLoggingPrivateStopFunction::WebrtcLoggingPrivateStopFunction() {}

WebrtcLoggingPrivateStopFunction::~WebrtcLoggingPrivateStopFunction() {}

bool WebrtcLoggingPrivateStopFunction::RunImpl() {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
  return false;
}

void WebrtcLoggingPrivateStopFunction::StopCallback(bool success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // TODO(grunell): Implement set lastError.
  NOTIMPLEMENTED();
  SendResponse(success);
}

WebrtcLoggingPrivateUploadFunction::WebrtcLoggingPrivateUploadFunction() {}

WebrtcLoggingPrivateUploadFunction::~WebrtcLoggingPrivateUploadFunction() {}

bool WebrtcLoggingPrivateUploadFunction::RunImpl() {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
  return false;
}

void WebrtcLoggingPrivateUploadFunction::UploadCallback(
    bool success, std::string report_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  api::webrtc_logging_private::UploadResult result;
  result.report_id = report_id;
  // TODO(grunell): Implement set lastError.
  NOTIMPLEMENTED();
  SendResponse(success);
}

WebrtcLoggingPrivateDiscardFunction::WebrtcLoggingPrivateDiscardFunction() {}

WebrtcLoggingPrivateDiscardFunction::~WebrtcLoggingPrivateDiscardFunction() {}

bool WebrtcLoggingPrivateDiscardFunction::RunImpl() {
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
  return false;
}

void WebrtcLoggingPrivateDiscardFunction::DiscardCallback(bool success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // TODO(grunell): Implement set lastError.
  NOTIMPLEMENTED();
  SendResponse(success);
}

}  // namespace extensions
