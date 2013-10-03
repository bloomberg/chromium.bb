// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webrtc_logging_private/webrtc_logging_private_api.h"

#include "base/logging.h"
#include "base/supports_user_data.h"
#include "chrome/browser/media/webrtc_logging_handler_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;

namespace extensions {

namespace SetMetaData = api::webrtc_logging_private::SetMetaData;
namespace SetUploadOnRenderClose =
    api::webrtc_logging_private::SetUploadOnRenderClose;

using api::webrtc_logging_private::MetaDataEntry;

WebrtcLoggingPrivateSetMetaDataFunction::
WebrtcLoggingPrivateSetMetaDataFunction() {}

WebrtcLoggingPrivateSetMetaDataFunction::
~WebrtcLoggingPrivateSetMetaDataFunction() {}

bool WebrtcLoggingPrivateSetMetaDataFunction::RunImpl() {
  scoped_ptr<SetMetaData::Params> params(SetMetaData::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderProcessHost* host = render_view_host()->GetProcess();
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(host, host));

  std::map<std::string, std::string> meta_data;
  for (std::vector<linked_ptr<MetaDataEntry> >::const_iterator it =
       params->meta_data.begin(); it != params->meta_data.end(); ++it) {
    meta_data[(*it)->key] = (*it)->value;
  }

  WebRtcLoggingHandlerHost::GenericDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateSetMetaDataFunction::SetMetaDataCallback, this);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::SetMetaData, webrtc_logging_handler_host,
      meta_data, callback));

  return true;
}

void WebrtcLoggingPrivateSetMetaDataFunction::SetMetaDataCallback(
    bool success, const std::string& error_message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!success)
    SetError(error_message);
  SendResponse(success);
}

WebrtcLoggingPrivateStartFunction::WebrtcLoggingPrivateStartFunction() {}

WebrtcLoggingPrivateStartFunction::~WebrtcLoggingPrivateStartFunction() {}

bool WebrtcLoggingPrivateStartFunction::RunImpl() {
  content::RenderProcessHost* host = render_view_host()->GetProcess();
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(host, host));

  WebRtcLoggingHandlerHost::GenericDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateStartFunction::StartCallback, this);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::StartLogging, webrtc_logging_handler_host,
      callback));

  return true;
}

void WebrtcLoggingPrivateStartFunction::StartCallback(
    bool success, const std::string& error_message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!success)
    SetError(error_message);
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

  content::RenderProcessHost* host = render_view_host()->GetProcess();
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(host, host));

  webrtc_logging_handler_host->set_upload_log_on_render_close(
      params->should_upload);

  return true;
}

WebrtcLoggingPrivateStopFunction::WebrtcLoggingPrivateStopFunction() {}

WebrtcLoggingPrivateStopFunction::~WebrtcLoggingPrivateStopFunction() {}

bool WebrtcLoggingPrivateStopFunction::RunImpl() {
  content::RenderProcessHost* host = render_view_host()->GetProcess();
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(host, host));

  WebRtcLoggingHandlerHost::GenericDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateStopFunction::StopCallback, this);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::StopLogging, webrtc_logging_handler_host,
      callback));

  return true;
}

void WebrtcLoggingPrivateStopFunction::StopCallback(
    bool success, const std::string& error_message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!success)
    SetError(error_message);
  SendResponse(success);
}

WebrtcLoggingPrivateUploadFunction::WebrtcLoggingPrivateUploadFunction() {}

WebrtcLoggingPrivateUploadFunction::~WebrtcLoggingPrivateUploadFunction() {}

bool WebrtcLoggingPrivateUploadFunction::RunImpl() {
  content::RenderProcessHost* host = render_view_host()->GetProcess();
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(host, host));

  WebRtcLoggingHandlerHost::UploadDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateUploadFunction::UploadCallback, this);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::UploadLog, webrtc_logging_handler_host,
      callback));

  return true;
}

void WebrtcLoggingPrivateUploadFunction::UploadCallback(
    bool success, const std::string& report_id,
    const std::string& error_message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (success) {
    api::webrtc_logging_private::UploadResult result;
    result.report_id = report_id;
    SetResult(result.ToValue().release());
  } else {
    SetError(error_message);
  }
  SendResponse(success);
}

WebrtcLoggingPrivateDiscardFunction::WebrtcLoggingPrivateDiscardFunction() {}

WebrtcLoggingPrivateDiscardFunction::~WebrtcLoggingPrivateDiscardFunction() {}

bool WebrtcLoggingPrivateDiscardFunction::RunImpl() {
  content::RenderProcessHost* host = render_view_host()->GetProcess();
  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(host, host));

  WebRtcLoggingHandlerHost::GenericDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateDiscardFunction::DiscardCallback, this);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &WebRtcLoggingHandlerHost::DiscardLog, webrtc_logging_handler_host,
      callback));

  return true;
}

void WebrtcLoggingPrivateDiscardFunction::DiscardCallback(
    bool success, const std::string& error_message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!success)
    SetError(error_message);
  SendResponse(success);
}

}  // namespace extensions
