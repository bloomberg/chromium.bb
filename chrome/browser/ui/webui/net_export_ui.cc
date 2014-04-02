// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_export_ui.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/net_log_temp_file.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/intent_helper.h"
#endif

using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateNetExportHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINetExportHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("net_export.js", IDR_NET_EXPORT_JS);
  source->SetDefaultResource(IDR_NET_EXPORT_HTML);
  return source;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's public methods are expected to run on the UI thread. All static
// functions except SendEmail run on FILE_USER_BLOCKING thread.
class NetExportMessageHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<NetExportMessageHandler> {
 public:
  NetExportMessageHandler();
  virtual ~NetExportMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Messages.
  void OnGetExportNetLogInfo(const base::ListValue* list);
  void OnStartNetLog(const base::ListValue* list);
  void OnStopNetLog(const base::ListValue* list);
  void OnSendNetLog(const base::ListValue* list);

 private:
  // Calls NetLogTempFile's ProcessCommand with DO_START and DO_STOP commands.
  static void ProcessNetLogCommand(
      base::WeakPtr<NetExportMessageHandler> net_export_message_handler,
      NetLogTempFile* net_log_temp_file,
      NetLogTempFile::Command command);

  // Returns the path to the file which has NetLog data.
  static base::FilePath GetNetLogFileName(NetLogTempFile* net_log_temp_file);

  // Send state/file information from NetLogTempFile.
  static void SendExportNetLogInfo(
      base::WeakPtr<NetExportMessageHandler> net_export_message_handler,
      NetLogTempFile* net_log_temp_file);

  // Send NetLog data via email. This runs on UI thread.
  static void SendEmail(const base::FilePath& file_to_send);

  // Call NetExportView.onExportNetLogInfoChanged JavsScript function in the
  // renderer, passing in |arg|. Takes ownership of |arg|.
  void OnExportNetLogInfoChanged(base::Value* arg);

  // Cache of g_browser_process->net_log()->net_log_temp_file().
  NetLogTempFile* net_log_temp_file_;

  base::WeakPtrFactory<NetExportMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetExportMessageHandler);
};

NetExportMessageHandler::NetExportMessageHandler()
    : net_log_temp_file_(g_browser_process->net_log()->net_log_temp_file()),
      weak_ptr_factory_(this) {
}

NetExportMessageHandler::~NetExportMessageHandler() {
  // Cancel any in-progress requests to collect net_log into temporary file.
  BrowserThread::PostTask(
      BrowserThread::FILE_USER_BLOCKING,
      FROM_HERE,
      base::Bind(&NetLogTempFile::ProcessCommand,
                 base::Unretained(net_log_temp_file_),
                 NetLogTempFile::DO_STOP));
}

void NetExportMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "getExportNetLogInfo",
      base::Bind(&NetExportMessageHandler::OnGetExportNetLogInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startNetLog",
      base::Bind(&NetExportMessageHandler::OnStartNetLog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "stopNetLog",
      base::Bind(&NetExportMessageHandler::OnStopNetLog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "sendNetLog",
      base::Bind(&NetExportMessageHandler::OnSendNetLog,
                 base::Unretained(this)));
}

void NetExportMessageHandler::OnGetExportNetLogInfo(
    const base::ListValue* list) {
  BrowserThread::PostTask(
      BrowserThread::FILE_USER_BLOCKING,
      FROM_HERE,
      base::Bind(&NetExportMessageHandler::SendExportNetLogInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 net_log_temp_file_));
}

void NetExportMessageHandler::OnStartNetLog(const base::ListValue* list) {
  bool strip_private_data = false;
  if (!list->GetBoolean(0, &strip_private_data)) {
    NOTREACHED() << "Failed to convert argument 1";
    return;
  }
  ProcessNetLogCommand(weak_ptr_factory_.GetWeakPtr(),
                       net_log_temp_file_,
                       (strip_private_data ?
                            NetLogTempFile::DO_START_STRIP_PRIVATE_DATA :
                            NetLogTempFile::DO_START));
}

void NetExportMessageHandler::OnStopNetLog(const base::ListValue* list) {
  ProcessNetLogCommand(weak_ptr_factory_.GetWeakPtr(),
                       net_log_temp_file_,
                       NetLogTempFile::DO_STOP);
}

void NetExportMessageHandler::OnSendNetLog(const base::ListValue* list) {
  content::BrowserThread::PostTaskAndReplyWithResult(
    content::BrowserThread::FILE_USER_BLOCKING,
        FROM_HERE,
        base::Bind(&NetExportMessageHandler::GetNetLogFileName,
                   base::Unretained(net_log_temp_file_)),
        base::Bind(&NetExportMessageHandler::SendEmail));
}

// static
void NetExportMessageHandler::ProcessNetLogCommand(
    base::WeakPtr<NetExportMessageHandler> net_export_message_handler,
    NetLogTempFile* net_log_temp_file,
    NetLogTempFile::Command command) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING)) {
    BrowserThread::PostTask(
        BrowserThread::FILE_USER_BLOCKING,
        FROM_HERE,
        base::Bind(&NetExportMessageHandler::ProcessNetLogCommand,
                   net_export_message_handler,
                   net_log_temp_file,
                   command));
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::FILE_USER_BLOCKING);
  net_log_temp_file->ProcessCommand(command);
  SendExportNetLogInfo(net_export_message_handler, net_log_temp_file);
}

// static
base::FilePath NetExportMessageHandler::GetNetLogFileName(
    NetLogTempFile* net_log_temp_file) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE_USER_BLOCKING);
  base::FilePath net_export_file_path;
  net_log_temp_file->GetFilePath(&net_export_file_path);
  return net_export_file_path;
}

// static
void NetExportMessageHandler::SendExportNetLogInfo(
    base::WeakPtr<NetExportMessageHandler> net_export_message_handler,
    NetLogTempFile* net_log_temp_file) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE_USER_BLOCKING);
  base::Value* value = net_log_temp_file->GetState();
  if (!BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NetExportMessageHandler::OnExportNetLogInfoChanged,
                 net_export_message_handler,
                 value))) {
    // Failed posting the task, avoid leaking.
    delete value;
  }
}

// static
void NetExportMessageHandler::SendEmail(const base::FilePath& file_to_send) {
  if (file_to_send.empty())
    return;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_ANDROID)
  std::string email;
  std::string subject = "net_internals_log";
  std::string title = "Issue number: ";
  std::string body =
      "Please add some informative text about the network issues.";
  base::FilePath::StringType file_to_attach(file_to_send.value());
  chrome::android::SendEmail(
      base::UTF8ToUTF16(email), base::UTF8ToUTF16(subject),
      base::UTF8ToUTF16(body), base::UTF8ToUTF16(title),
      base::UTF8ToUTF16(file_to_attach));
#endif
}

void NetExportMessageHandler::OnExportNetLogInfoChanged(base::Value* arg) {
  scoped_ptr<base::Value> value(arg);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_ui()->CallJavascriptFunction(
      "NetExportView.getInstance().onExportNetLogInfoChanged", *arg);
}

}  // namespace

NetExportUI::NetExportUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new NetExportMessageHandler());

  // Set up the chrome://net-export/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateNetExportHTMLSource());
}
