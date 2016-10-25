// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_export_ui.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/features.h"
#include "chrome/common/url_constants.h"
#include "components/grit/components_resources.h"
#include "components/net_log/chrome_net_log.h"
#include "components/net_log/net_export_ui_constants.h"
#include "components/net_log/net_log_file_writer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
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
  source->AddResourcePath(net_log::kNetExportUIJS, IDR_NET_LOG_NET_EXPORT_JS);
  source->SetDefaultResource(IDR_NET_LOG_NET_EXPORT_HTML);
  return source;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's public methods are expected to run on the UI thread. All static
// functions except SendEmail run on FILE_USER_BLOCKING thread.
class NetExportMessageHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<NetExportMessageHandler>,
      public ui::SelectFileDialog::Listener {
 public:
  NetExportMessageHandler();
  ~NetExportMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Messages.
  void OnGetExportNetLogInfo(const base::ListValue* list);
  void OnStartNetLog(const base::ListValue* list);
  void OnStopNetLog(const base::ListValue* list);
  void OnSendNetLog(const base::ListValue* list);

  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  // Calls NetLogFileWriter's ProcessCommand with DO_START and DO_STOP commands.
  static void ProcessNetLogCommand(
      base::WeakPtr<NetExportMessageHandler> net_export_message_handler,
      net_log::NetLogFileWriter* net_log_file_writer,
      net_log::NetLogFileWriter::Command command);

  // Returns the path to the file which has NetLog data.
  static base::FilePath GetNetLogFileName(
      net_log::NetLogFileWriter* net_log_file_writer);

  // Send state/file information from NetLogFileWriter.
  static void SendExportNetLogInfo(
      base::WeakPtr<NetExportMessageHandler> net_export_message_handler,
      net_log::NetLogFileWriter* net_log_file_writer);

  // Send NetLog data via email. This runs on UI thread.
  static void SendEmail(const base::FilePath& file_to_send);

  // chrome://net-export can be used on both mobile and desktop platforms.
  // On mobile a user cannot pick where their NetLog file is saved to.
  // Instead, everything is saved on the user's temp directory. Thus the
  // mobile user has the UI available to send their NetLog file as an
  // email while the desktop user, who gets to chose their NetLog file's
  // location, does not. Furthermore, since every time a user starts logging
  // to a new NetLog file on mobile platforms it overwrites the previous
  // NetLog file, a warning message appears on the Start Logging button
  // that informs the user of this. This does not exist on the desktop
  // UI.
  static bool UsingMobileUI();

  // Sets the correct start command and sends this to ProcessNetLogCommand.
  void StartNetLog();

  // Call NetExportView.onExportNetLogInfoChanged JavsScript function in the
  // renderer, passing in |arg|. Takes ownership of |arg|.
  void OnExportNetLogInfoChanged(base::Value* arg);

  // Opens the SelectFileDialog UI with the default path to save a
  // NetLog file.
  void ShowSelectFileDialog(const base::FilePath& default_path);

  // Cache of g_browser_process->net_log()->net_log_file_writer(). This
  // is owned by ChromeNetLog which is owned by BrowserProcessImpl. There are
  // four instances in this class where a pointer to net_log_file_writer_ is
  // posted to the FILE_USER_BLOCKING thread. Base::Unretained is used here
  // because BrowserProcessImpl is destroyed on the UI thread after joining the
  // FILE_USER_BLOCKING thread making it impossible for there to be an invalid
  // pointer to this object when going back to the UI thread. Furthermore this
  // pointer is never dereferenced prematurely on the UI thread. Thus the
  // lifetime of this object is assured and can be safely used with
  // base::Unretained.
  net_log::NetLogFileWriter* net_log_file_writer_;

  std::string log_mode_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  base::WeakPtrFactory<NetExportMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetExportMessageHandler);
};

NetExportMessageHandler::NetExportMessageHandler()
    : net_log_file_writer_(g_browser_process->net_log()->net_log_file_writer()),
      weak_ptr_factory_(this) {}

NetExportMessageHandler::~NetExportMessageHandler() {
  // There may be a pending file dialog, it needs to be told that the user
  // has gone away so that it doesn't try to call back.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  // Cancel any in-progress requests to collect net_log into a file.
  BrowserThread::PostTask(BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
                          base::Bind(&net_log::NetLogFileWriter::ProcessCommand,
                                     base::Unretained(net_log_file_writer_),
                                     net_log::NetLogFileWriter::DO_STOP));
}

void NetExportMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      net_log::kGetExportNetLogInfoHandler,
      base::Bind(&NetExportMessageHandler::OnGetExportNetLogInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kStartNetLogHandler,
      base::Bind(&NetExportMessageHandler::OnStartNetLog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kStopNetLogHandler,
      base::Bind(&NetExportMessageHandler::OnStopNetLog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kSendNetLogHandler,
      base::Bind(&NetExportMessageHandler::OnSendNetLog,
                 base::Unretained(this)));
}

void NetExportMessageHandler::OnGetExportNetLogInfo(
    const base::ListValue* list) {
  BrowserThread::PostTask(
      BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(&NetExportMessageHandler::SendExportNetLogInfo,
                 weak_ptr_factory_.GetWeakPtr(), net_log_file_writer_));
}

void NetExportMessageHandler::OnStartNetLog(const base::ListValue* list) {
  bool result = list->GetString(0, &log_mode_);
  DCHECK(result);

  if (UsingMobileUI()) {
    StartNetLog();
  } else {
    base::FilePath home_dir = base::GetHomeDir();
    base::FilePath default_path =
        home_dir.Append(FILE_PATH_LITERAL("chrome-net-export-log.json"));
    ShowSelectFileDialog(default_path);
  }
}

void NetExportMessageHandler::OnStopNetLog(const base::ListValue* list) {
  ProcessNetLogCommand(weak_ptr_factory_.GetWeakPtr(), net_log_file_writer_,
                       net_log::NetLogFileWriter::DO_STOP);
}

void NetExportMessageHandler::OnSendNetLog(const base::ListValue* list) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(&NetExportMessageHandler::GetNetLogFileName,
                 base::Unretained(net_log_file_writer_)),
      base::Bind(&NetExportMessageHandler::SendEmail));
}

void NetExportMessageHandler::StartNetLog() {
  net_log::NetLogFileWriter::Command command;
  if (log_mode_ == "LOG_BYTES") {
    command = net_log::NetLogFileWriter::DO_START_LOG_BYTES;
  } else if (log_mode_ == "NORMAL") {
    command = net_log::NetLogFileWriter::DO_START;
  } else {
    DCHECK_EQ("STRIP_PRIVATE_DATA", log_mode_);
    command = net_log::NetLogFileWriter::DO_START_STRIP_PRIVATE_DATA;
  }

  ProcessNetLogCommand(weak_ptr_factory_.GetWeakPtr(), net_log_file_writer_,
                       command);
}

// static
void NetExportMessageHandler::ProcessNetLogCommand(
    base::WeakPtr<NetExportMessageHandler> net_export_message_handler,
    net_log::NetLogFileWriter* net_log_file_writer,
    net_log::NetLogFileWriter::Command command) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING)) {
    BrowserThread::PostTask(
        BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
        base::Bind(&NetExportMessageHandler::ProcessNetLogCommand,
                   net_export_message_handler, net_log_file_writer, command));
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::FILE_USER_BLOCKING);
  net_log_file_writer->ProcessCommand(command);
  SendExportNetLogInfo(net_export_message_handler, net_log_file_writer);
}

// static
base::FilePath NetExportMessageHandler::GetNetLogFileName(
    net_log::NetLogFileWriter* net_log_file_writer) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE_USER_BLOCKING);
  base::FilePath net_export_file_path;
  net_log_file_writer->GetFilePath(&net_export_file_path);
  return net_export_file_path;
}

// static
void NetExportMessageHandler::SendExportNetLogInfo(
    base::WeakPtr<NetExportMessageHandler> net_export_message_handler,
    net_log::NetLogFileWriter* net_log_file_writer) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE_USER_BLOCKING);
  base::DictionaryValue* dict = net_log_file_writer->GetState();
  dict->SetBoolean("useMobileUI", UsingMobileUI());
  base::Value* value = dict;
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

#if BUILDFLAG(ANDROID_JAVA_UI)
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

// static
bool NetExportMessageHandler::UsingMobileUI() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  return true;
#else
  return false;
#endif
}

void NetExportMessageHandler::OnExportNetLogInfoChanged(base::Value* arg) {
  std::unique_ptr<base::Value> value(arg);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_ui()->CallJavascriptFunctionUnsafe(net_log::kOnExportNetLogInfoChanged,
                                         *arg);
}

void NetExportMessageHandler::ShowSelectFileDialog(
    const base::FilePath& default_path) {
  // User may have clicked more than once before the save dialog appears.
  // This prevents creating more than one save dialog.
  if (select_file_dialog_)
    return;

  WebContents* webcontents = web_ui()->GetWebContents();

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(webcontents));
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {{FILE_PATH_LITERAL("json")}};
  gfx::NativeWindow owning_window = webcontents->GetTopLevelNativeWindow();
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, base::string16(), default_path,
      &file_type_info, 0, base::FilePath::StringType(), owning_window, nullptr);
}

void NetExportMessageHandler::FileSelected(const base::FilePath& path,
                                           int index,
                                           void* params) {
  DCHECK(select_file_dialog_);
  select_file_dialog_ = nullptr;

  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(&net_log::NetLogFileWriter::SetUpNetExportLogPath,
                 base::Unretained(net_log_file_writer_), path),
      // NetExportMessageHandler is tied to the lifetime of the tab
      // so it cannot be assured that it will be valid when this
      // StartNetLog is called. Instead of using base::Unretained a
      // weak pointer is used to adjust for this.
      base::Bind(&NetExportMessageHandler::StartNetLog,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetExportMessageHandler::FileSelectionCanceled(void* params) {
  DCHECK(select_file_dialog_);
  select_file_dialog_ = nullptr;
}

}  // namespace

NetExportUI::NetExportUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new NetExportMessageHandler());

  // Set up the chrome://net-export/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateNetExportHTMLSource());
}
