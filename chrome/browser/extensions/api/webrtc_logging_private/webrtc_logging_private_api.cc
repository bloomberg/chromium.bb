// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/api/webrtc_logging_private/webrtc_logging_private_api.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/webrtc_logging_handler_host.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/error_utils.h"

using content::BrowserThread;

namespace extensions {

namespace SetMetaData = api::webrtc_logging_private::SetMetaData;
namespace Start = api::webrtc_logging_private::Start;
namespace SetUploadOnRenderClose =
    api::webrtc_logging_private::SetUploadOnRenderClose;
namespace Stop = api::webrtc_logging_private::Stop;
namespace Upload = api::webrtc_logging_private::Upload;
namespace Discard = api::webrtc_logging_private::Discard;
namespace StartRtpDump = api::webrtc_logging_private::StartRtpDump;
namespace StopRtpDump = api::webrtc_logging_private::StopRtpDump;

using api::webrtc_logging_private::MetaDataEntry;

content::RenderProcessHost*
WebrtcLoggingPrivateTabIdFunction::RphFromTabIdAndSecurityOrigin(
    int tab_id, const std::string& security_origin) {
  content::WebContents* contents = NULL;
  if (!ExtensionTabUtil::GetTabById(
           tab_id, GetProfile(), true, NULL, NULL, &contents, NULL)) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        extensions::tabs_constants::kTabNotFoundError,
        base::IntToString(tab_id));
    return NULL;
  }
  if (!contents) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        "Web contents for tab not found",
        base::IntToString(tab_id));
    return NULL;
  }
  if (contents->GetURL().GetOrigin().spec() != security_origin) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        "Invalid security origin",
        base::IntToString(tab_id));
    return NULL;
  }
  return contents->GetRenderProcessHost();
}

WebrtcLoggingPrivateSetMetaDataFunction::
WebrtcLoggingPrivateSetMetaDataFunction() {}

WebrtcLoggingPrivateSetMetaDataFunction::
~WebrtcLoggingPrivateSetMetaDataFunction() {}

bool WebrtcLoggingPrivateSetMetaDataFunction::RunAsync() {
  scoped_ptr<SetMetaData::Params> params(SetMetaData::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderProcessHost* host =
      RphFromTabIdAndSecurityOrigin(params->tab_id, params->security_origin);
  if (!host)
    return false;

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success)
    SetError(error_message);
  SendResponse(success);
}

WebrtcLoggingPrivateStartFunction::WebrtcLoggingPrivateStartFunction() {}

WebrtcLoggingPrivateStartFunction::~WebrtcLoggingPrivateStartFunction() {}

bool WebrtcLoggingPrivateStartFunction::RunAsync() {
  scoped_ptr<Start::Params> params(Start::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderProcessHost* host =
      RphFromTabIdAndSecurityOrigin(params->tab_id, params->security_origin);
  if (!host)
    return false;

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success)
    SetError(error_message);
  SendResponse(success);
}

WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::
WebrtcLoggingPrivateSetUploadOnRenderCloseFunction() {}

WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::
~WebrtcLoggingPrivateSetUploadOnRenderCloseFunction() {}

bool WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::RunAsync() {
  scoped_ptr<SetUploadOnRenderClose::Params> params(
      SetUploadOnRenderClose::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderProcessHost* host =
      RphFromTabIdAndSecurityOrigin(params->tab_id, params->security_origin);
  if (!host)
    return false;

  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(host, host));

  webrtc_logging_handler_host->set_upload_log_on_render_close(
      params->should_upload);

  return true;
}

WebrtcLoggingPrivateStopFunction::WebrtcLoggingPrivateStopFunction() {}

WebrtcLoggingPrivateStopFunction::~WebrtcLoggingPrivateStopFunction() {}

bool WebrtcLoggingPrivateStopFunction::RunAsync() {
  scoped_ptr<Stop::Params> params(Stop::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderProcessHost* host =
      RphFromTabIdAndSecurityOrigin(params->tab_id, params->security_origin);
  if (!host)
    return false;

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success)
    SetError(error_message);
  SendResponse(success);
}

WebrtcLoggingPrivateUploadFunction::WebrtcLoggingPrivateUploadFunction() {}

WebrtcLoggingPrivateUploadFunction::~WebrtcLoggingPrivateUploadFunction() {}

bool WebrtcLoggingPrivateUploadFunction::RunAsync() {
  scoped_ptr<Upload::Params> params(Upload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderProcessHost* host =
      RphFromTabIdAndSecurityOrigin(params->tab_id, params->security_origin);
  if (!host)
    return false;

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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

bool WebrtcLoggingPrivateDiscardFunction::RunAsync() {
  scoped_ptr<Discard::Params> params(Discard::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderProcessHost* host =
      RphFromTabIdAndSecurityOrigin(params->tab_id, params->security_origin);
  if (!host)
    return false;

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success)
    SetError(error_message);
  SendResponse(success);
}

WebrtcLoggingPrivateStartRtpDumpFunction::
    WebrtcLoggingPrivateStartRtpDumpFunction() {}

WebrtcLoggingPrivateStartRtpDumpFunction::
    ~WebrtcLoggingPrivateStartRtpDumpFunction() {}

bool WebrtcLoggingPrivateStartRtpDumpFunction::RunAsync() {
  scoped_ptr<StartRtpDump::Params> params(StartRtpDump::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->incoming && !params->outgoing) {
    StartRtpDumpCallback(false, "Either incoming or outgoing must be true.");
    return true;
  }

  RtpDumpType type =
      (params->incoming && params->outgoing)
          ? RTP_DUMP_BOTH
          : (params->incoming ? RTP_DUMP_INCOMING : RTP_DUMP_OUTGOING);

  content::RenderProcessHost* host =
      RphFromTabIdAndSecurityOrigin(params->tab_id, params->security_origin);
  if (!host)
    return false;

  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(host, host));

  WebRtcLoggingHandlerHost::GenericDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateStartRtpDumpFunction::StartRtpDumpCallback, this);

  // This call cannot fail.
  content::RenderProcessHost::WebRtcStopRtpDumpCallback stop_callback =
      host->StartRtpDump(params->incoming,
                         params->outgoing,
                         base::Bind(&WebRtcLoggingHandlerHost::OnRtpPacket,
                                    webrtc_logging_handler_host));

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&WebRtcLoggingHandlerHost::StartRtpDump,
                                     webrtc_logging_handler_host,
                                     type,
                                     callback,
                                     stop_callback));
  return true;
}

void WebrtcLoggingPrivateStartRtpDumpFunction::StartRtpDumpCallback(
    bool success,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success)
    SetError(error_message);
  SendResponse(success);
}

WebrtcLoggingPrivateStopRtpDumpFunction::
    WebrtcLoggingPrivateStopRtpDumpFunction() {}

WebrtcLoggingPrivateStopRtpDumpFunction::
    ~WebrtcLoggingPrivateStopRtpDumpFunction() {}

bool WebrtcLoggingPrivateStopRtpDumpFunction::RunAsync() {
  scoped_ptr<StopRtpDump::Params> params(StopRtpDump::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->incoming && !params->outgoing) {
    StopRtpDumpCallback(false, "Either incoming or outgoing must be true.");
    return true;
  }

  RtpDumpType type =
      (params->incoming && params->outgoing)
          ? RTP_DUMP_BOTH
          : (params->incoming ? RTP_DUMP_INCOMING : RTP_DUMP_OUTGOING);

  content::RenderProcessHost* host =
      RphFromTabIdAndSecurityOrigin(params->tab_id, params->security_origin);
  if (!host)
    return false;

  scoped_refptr<WebRtcLoggingHandlerHost> webrtc_logging_handler_host(
      base::UserDataAdapter<WebRtcLoggingHandlerHost>::Get(host, host));

  WebRtcLoggingHandlerHost::GenericDoneCallback callback = base::Bind(
      &WebrtcLoggingPrivateStopRtpDumpFunction::StopRtpDumpCallback, this);

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&WebRtcLoggingHandlerHost::StopRtpDump,
                                     webrtc_logging_handler_host,
                                     type,
                                     callback));
  return true;
}

void WebrtcLoggingPrivateStopRtpDumpFunction::StopRtpDumpCallback(
    bool success,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success)
    SetError(error_message);
  SendResponse(success);
}

}  // namespace extensions
