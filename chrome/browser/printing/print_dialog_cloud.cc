// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/printing/print_dialog_cloud_internal.h"

#include "base/base64.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/browser/webui/web_ui.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webpreferences.h"

#include "grit/generated_resources.h"

// This module implements the UI support in Chrome for cloud printing.
// This means hosting a dialog containing HTML/JavaScript and using
// the published cloud print user interface integration APIs to get
// page setup settings from the dialog contents and provide the
// generated print PDF to the dialog contents for uploading to the
// cloud print service.

// Currently, the flow between these classes is as follows:

// PrintDialogCloud::CreatePrintDialogForPdf is called from
// resource_message_filter_gtk.cc once the renderer has informed the
// renderer host that PDF generation into the renderer host provided
// temp file has been completed.  That call is on the FILE thread.
// That, in turn, hops over to the UI thread to create an instance of
// PrintDialogCloud.

// The constructor for PrintDialogCloud creates a
// CloudPrintHtmlDialogDelegate and asks the current active browser to
// show an HTML dialog using that class as the delegate. That class
// hands in the kCloudPrintResourcesURL as the URL to visit.  That is
// recognized by the GetWebUIFactoryFunction as a signal to create an
// ExternalHtmlDialogUI.

// CloudPrintHtmlDialogDelegate also temporarily owns a
// CloudPrintFlowHandler, a class which is responsible for the actual
// interactions with the dialog contents, including handing in the PDF
// print data and getting any page setup parameters that the dialog
// contents provides.  As part of bringing up the dialog,
// HtmlDialogUI::RenderViewCreated is called (an override of
// WebUI::RenderViewCreated).  That routine, in turn, calls the
// delegate's GetWebUIMessageHandlers routine, at which point the
// ownership of the CloudPrintFlowHandler is handed over.  A pointer
// to the flow handler is kept to facilitate communication back and
// forth between the two classes.

// The WebUI continues dialog bring-up, calling
// CloudPrintFlowHandler::RegisterMessages.  This is where the
// additional object model capabilities are registered for the dialog
// contents to use.  It is also at this time that capabilities for the
// dialog contents are adjusted to allow the dialog contents to close
// the window.  In addition, the pending URL is redirected to the
// actual cloud print service URL.  The flow controller also registers
// for notification of when the dialog contents finish loading, which
// is currently used to send the PDF data to the dialog contents.

// In order to send the PDF data to the dialog contents, the flow
// handler uses a CloudPrintDataSender.  It creates one, letting it
// know the name of the temporary file containing the PDF data, and
// posts the task of reading the file
// (CloudPrintDataSender::ReadPrintDataFile) to the file thread.  That
// routine reads in the file, and then hops over to the IO thread to
// send that data to the dialog contents.

// When the dialog contents are finished (by either being cancelled or
// hitting the print button), the delegate is notified, and responds
// that the dialog should be closed, at which point things are torn
// down and released.

// TODO(scottbyer):
// http://code.google.com/p/chromium/issues/detail?id=44093 The
// high-level flow (where the PDF data is generated before even
// bringing up the dialog) isn't what we want.

namespace internal_cloud_print_helpers {

bool GetDoubleOrInt(const DictionaryValue& dictionary,
                    const std::string& path,
                    double* out_value) {
  if (!dictionary.GetDouble(path, out_value)) {
    int int_value = 0;
    if (!dictionary.GetInteger(path, &int_value))
      return false;
    *out_value = int_value;
  }
  return true;
}

// From the JSON parsed value, get the entries for the page setup
// parameters.
bool GetPageSetupParameters(const std::string& json,
                            ViewMsg_Print_Params& parameters) {
  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json, false));
  DLOG_IF(ERROR, (!parsed_value.get() ||
                  !parsed_value->IsType(Value::TYPE_DICTIONARY)))
      << "PageSetup call didn't have expected contents";
  if (!parsed_value.get() || !parsed_value->IsType(Value::TYPE_DICTIONARY))
    return false;

  bool result = true;
  DictionaryValue* params = static_cast<DictionaryValue*>(parsed_value.get());
  result &= GetDoubleOrInt(*params, "dpi", &parameters.dpi);
  result &= GetDoubleOrInt(*params, "min_shrink", &parameters.min_shrink);
  result &= GetDoubleOrInt(*params, "max_shrink", &parameters.max_shrink);
  result &= params->GetBoolean("selection_only", &parameters.selection_only);
  return result;
}

void CloudPrintDataSenderHelper::CallJavascriptFunction(
    const std::wstring& function_name) {
  web_ui_->CallJavascriptFunction(WideToASCII(function_name));
}

void CloudPrintDataSenderHelper::CallJavascriptFunction(
    const std::wstring& function_name, const Value& arg) {
  web_ui_->CallJavascriptFunction(WideToASCII(function_name), arg);
}

void CloudPrintDataSenderHelper::CallJavascriptFunction(
    const std::wstring& function_name, const Value& arg1, const Value& arg2) {
  web_ui_->CallJavascriptFunction(WideToASCII(function_name), arg1, arg2);
}

// Clears out the pointer we're using to communicate.  Either routine is
// potentially expensive enough that stopping whatever is in progress
// is worth it.
void CloudPrintDataSender::CancelPrintDataFile() {
  base::AutoLock lock(lock_);
  // We don't own helper, it was passed in to us, so no need to
  // delete, just let it go.
  helper_ = NULL;
}

CloudPrintDataSender::CloudPrintDataSender(CloudPrintDataSenderHelper* helper,
                                           const string16& print_job_title)
    : helper_(helper),
      print_job_title_(print_job_title) {
}

CloudPrintDataSender::~CloudPrintDataSender() {}

// Grab the raw PDF file contents and massage them into shape for
// sending to the dialog contents (and up to the cloud print server)
// by encoding it and prefixing it with the appropriate mime type.
// Once that is done, kick off the next part of the task on the IO
// thread.
void CloudPrintDataSender::ReadPrintDataFile(const FilePath& path_to_pdf) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  int64 file_size = 0;
  if (file_util::GetFileSize(path_to_pdf, &file_size) && file_size != 0) {
    std::string file_data;
    if (file_size < kuint32max) {
      file_data.reserve(static_cast<unsigned int>(file_size));
    } else {
      DLOG(WARNING) << " print data file too large to reserve space";
    }
    if (helper_ && file_util::ReadFileToString(path_to_pdf, &file_data)) {
      std::string base64_data;
      base::Base64Encode(file_data, &base64_data);
      std::string header("data:application/pdf;base64,");
      base64_data.insert(0, header);
      scoped_ptr<StringValue> new_data(new StringValue(base64_data));
      print_data_.swap(new_data);
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                              NewRunnableMethod(
                                  this,
                                  &CloudPrintDataSender::SendPrintDataFile));
    }
  }
}

// We have the data in hand that needs to be pushed into the dialog
// contents; do so from the IO thread.

// TODO(scottbyer): If the print data ends up being larger than the
// upload limit (currently 10MB), what we need to do is upload that
// large data to google docs and set the URL in the printing
// JavaScript to that location, and make sure it gets deleted when not
// needed. - 4/1/2010
void CloudPrintDataSender::SendPrintDataFile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::AutoLock lock(lock_);
  if (helper_ && print_data_.get()) {
    StringValue title(print_job_title_);

    // Send the print data to the dialog contents.  The JavaScript
    // function is a preliminary API for prototyping purposes and is
    // subject to change.
    const_cast<CloudPrintDataSenderHelper*>(helper_)->CallJavascriptFunction(
        L"printApp._printDataUrl", *print_data_, title);
  }
}


CloudPrintFlowHandler::CloudPrintFlowHandler(const FilePath& path_to_pdf,
                                             const string16& print_job_title)
    : path_to_pdf_(path_to_pdf),
      print_job_title_(print_job_title) {
}

CloudPrintFlowHandler::~CloudPrintFlowHandler() {
  // This will also cancel any task in flight.
  CancelAnyRunningTask();
}


void CloudPrintFlowHandler::SetDialogDelegate(
    CloudPrintHtmlDialogDelegate* delegate) {
  // Even if setting a new WebUI, it means any previous task needs
  // to be cancelled, it's now invalid.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CancelAnyRunningTask();
  dialog_delegate_ = delegate;
}

// Cancels any print data sender we have in flight and removes our
// reference to it, so when the task that is calling it finishes and
// removes it's reference, it goes away.
void CloudPrintFlowHandler::CancelAnyRunningTask() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (print_data_sender_.get()) {
    print_data_sender_->CancelPrintDataFile();
    print_data_sender_ = NULL;
  }
}

void CloudPrintFlowHandler::RegisterMessages() {
  if (!web_ui_)
    return;

  // TODO(scottbyer) - This is where we will register messages for the
  // UI JS to use.  Needed: Call to update page setup parameters.
  web_ui_->RegisterMessageCallback(
      "ShowDebugger",
      NewCallback(this, &CloudPrintFlowHandler::HandleShowDebugger));
  web_ui_->RegisterMessageCallback(
      "SendPrintData",
      NewCallback(this, &CloudPrintFlowHandler::HandleSendPrintData));
  web_ui_->RegisterMessageCallback(
      "SetPageParameters",
      NewCallback(this, &CloudPrintFlowHandler::HandleSetPageParameters));

  if (web_ui_->tab_contents()) {
    // Also, take the opportunity to set some (minimal) additional
    // script permissions required for the web UI.

    // TODO(scottbyer): learn how to make sure we're talking to the
    // right web site first.
    RenderViewHost* rvh = web_ui_->tab_contents()->render_view_host();
    if (rvh && rvh->delegate()) {
      WebPreferences webkit_prefs = rvh->delegate()->GetWebkitPrefs();
      webkit_prefs.allow_scripts_to_close_windows = true;
      rvh->UpdateWebPreferences(webkit_prefs);
    }

    // Register for appropriate notifications, and re-direct the URL
    // to the real server URL, now that we've gotten an HTML dialog
    // going.
    NavigationController* controller = &web_ui_->tab_contents()->controller();
    NavigationEntry* pending_entry = controller->pending_entry();
    if (pending_entry)
      pending_entry->set_url(CloudPrintURL(
          web_ui_->GetProfile()).GetCloudPrintServiceDialogURL());
    registrar_.Add(this, NotificationType::LOAD_STOP,
                   Source<NavigationController>(controller));
  }
}

void CloudPrintFlowHandler::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  if (type == NotificationType::LOAD_STOP) {
    // Choose one or the other.  If you need to debug, bring up the
    // debugger.  You can then use the various chrome.send()
    // registrations above to kick of the various function calls,
    // including chrome.send("SendPrintData") in the javaScript
    // console and watch things happen with:
    // HandleShowDebugger(NULL);
    HandleSendPrintData(NULL);
  }
}

void CloudPrintFlowHandler::HandleShowDebugger(const ListValue* args) {
  ShowDebugger();
}

void CloudPrintFlowHandler::ShowDebugger() {
  if (web_ui_) {
    RenderViewHost* rvh = web_ui_->tab_contents()->render_view_host();
    if (rvh)
      DevToolsManager::GetInstance()->OpenDevToolsWindow(rvh);
  }
}

scoped_refptr<CloudPrintDataSender>
CloudPrintFlowHandler::CreateCloudPrintDataSender() {
  DCHECK(web_ui_);
  print_data_helper_.reset(new CloudPrintDataSenderHelper(web_ui_));
  return new CloudPrintDataSender(print_data_helper_.get(), print_job_title_);
}

void CloudPrintFlowHandler::HandleSendPrintData(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // This will cancel any ReadPrintDataFile() or SendPrintDataFile()
  // requests in flight (this is anticipation of when setting page
  // setup parameters becomes asynchronous and may be set while some
  // data is in flight).  Then we can clear out the print data.
  CancelAnyRunningTask();
  if (web_ui_) {
    print_data_sender_ = CreateCloudPrintDataSender();
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            NewRunnableMethod(
                                print_data_sender_.get(),
                                &CloudPrintDataSender::ReadPrintDataFile,
                                path_to_pdf_));
  }
}

void CloudPrintFlowHandler::HandleSetPageParameters(const ListValue* args) {
  std::string json;
  args->GetString(0, &json);
  if (json.empty()) {
    NOTREACHED() << "Empty json string";
    return;
  }

  // These are backstop default values - 72 dpi to match the screen,
  // 8.5x11 inch paper with margins subtracted (1/4 inch top, left,
  // right and 0.56 bottom), and the min page shrink and max page
  // shrink values appear all over the place with no explanation.

  // TODO(scottbyer): Get a Linux/ChromeOS edge for PrintSettings
  // working so that we can get the default values from there.  Fix up
  // PrintWebViewHelper to do the same.
  const int kDPI = 72;
  const int kWidth = static_cast<int>((8.5-0.25-0.25)*kDPI);
  const int kHeight = static_cast<int>((11-0.25-0.56)*kDPI);
  const double kMinPageShrink = 1.25;
  const double kMaxPageShrink = 2.0;

  ViewMsg_Print_Params default_settings;
  default_settings.printable_size = gfx::Size(kWidth, kHeight);
  default_settings.dpi = kDPI;
  default_settings.min_shrink = kMinPageShrink;
  default_settings.max_shrink = kMaxPageShrink;
  default_settings.desired_dpi = kDPI;
  default_settings.document_cookie = 0;
  default_settings.selection_only = false;

  if (!GetPageSetupParameters(json, default_settings)) {
    NOTREACHED();
    return;
  }

  // TODO(scottbyer) - Here is where we would kick the originating
  // renderer thread with these new parameters in order to get it to
  // re-generate the PDF and hand it back to us.  window.print() is
  // currently synchronous, so there's a lot of work to do to get to
  // that point.
}

void CloudPrintFlowHandler::StoreDialogClientSize() const {
  if (web_ui_ && web_ui_->tab_contents() && web_ui_->tab_contents()->view()) {
    gfx::Size size = web_ui_->tab_contents()->view()->GetContainerSize();
    web_ui_->GetProfile()->GetPrefs()->SetInteger(
        prefs::kCloudPrintDialogWidth, size.width());
    web_ui_->GetProfile()->GetPrefs()->SetInteger(
        prefs::kCloudPrintDialogHeight, size.height());
  }
}

CloudPrintHtmlDialogDelegate::CloudPrintHtmlDialogDelegate(
    const FilePath& path_to_pdf,
    int width, int height,
    const std::string& json_arguments,
    const string16& print_job_title,
    bool modal)
    : flow_handler_(new CloudPrintFlowHandler(path_to_pdf, print_job_title)),
      modal_(modal),
      owns_flow_handler_(true) {
  Init(width, height, json_arguments);
}

// For unit testing.
CloudPrintHtmlDialogDelegate::CloudPrintHtmlDialogDelegate(
    CloudPrintFlowHandler* flow_handler,
    int width, int height,
    const std::string& json_arguments,
    bool modal)
    : flow_handler_(flow_handler),
      modal_(modal),
      owns_flow_handler_(true) {
  Init(width, height, json_arguments);
}

void CloudPrintHtmlDialogDelegate::Init(int width, int height,
                                        const std::string& json_arguments) {
  // This information is needed to show the dialog HTML content.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  params_.url = GURL(chrome::kCloudPrintResourcesURL);
  params_.height = height;
  params_.width = width;
  params_.json_input = json_arguments;

  flow_handler_->SetDialogDelegate(this);
  // If we're not modal we can show the dialog with no browser.
  // We need this to keep Chrome alive while our dialog is up.
  if (!modal_)
    BrowserList::StartKeepAlive();
}

CloudPrintHtmlDialogDelegate::~CloudPrintHtmlDialogDelegate() {
  // If the flow_handler_ is about to outlive us because we don't own
  // it anymore, we need to have it remove it's reference to us.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  flow_handler_->SetDialogDelegate(NULL);
  if (owns_flow_handler_) {
    delete flow_handler_;
  }
}

bool CloudPrintHtmlDialogDelegate::IsDialogModal() const {
    return modal_;
}

std::wstring CloudPrintHtmlDialogDelegate::GetDialogTitle() const {
  return std::wstring();
}

GURL CloudPrintHtmlDialogDelegate::GetDialogContentURL() const {
  return params_.url;
}

void CloudPrintHtmlDialogDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(flow_handler_);
  // We don't own flow_handler_ anymore, but it sticks around until at
  // least right after OnDialogClosed() is called (and this object is
  // destroyed).
  owns_flow_handler_ = false;
}

void CloudPrintHtmlDialogDelegate::GetDialogSize(gfx::Size* size) const {
  size->set_width(params_.width);
  size->set_height(params_.height);
}

std::string CloudPrintHtmlDialogDelegate::GetDialogArgs() const {
  return params_.json_input;
}

void CloudPrintHtmlDialogDelegate::OnDialogClosed(
    const std::string& json_retval) {
  // Get the final dialog size and store it.
  flow_handler_->StoreDialogClientSize();
  // If we're modal we can show the dialog with no browser.
  // End the keep-alive so that Chrome can exit.
  if (!modal_)
    BrowserList::EndKeepAlive();
  delete this;
}

void CloudPrintHtmlDialogDelegate::OnCloseContents(TabContents* source,
                                                   bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool CloudPrintHtmlDialogDelegate::ShouldShowDialogTitle() const {
  return false;
}

}  // namespace internal_cloud_print_helpers

// static, called on the IO thread.  This is the main entry point into
// creating the dialog.

// TODO(scottbyer): The signature here will need to change as the
// workflow through the printing code changes to allow for dynamically
// changing page setup parameters while the dialog is active.
void PrintDialogCloud::CreatePrintDialogForPdf(const FilePath& path_to_pdf,
                                               const string16& print_job_title,
                                               bool modal) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&PrintDialogCloud::CreateDialogImpl,
                          path_to_pdf,
                          print_job_title,
                          modal));
}

// static, called from the UI thread.
void PrintDialogCloud::CreateDialogImpl(const FilePath& path_to_pdf,
                                        const string16& print_job_title,
                                        bool modal) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  new PrintDialogCloud(path_to_pdf, print_job_title, modal);
}

// Initialize the print dialog.  Called on the UI thread.
PrintDialogCloud::PrintDialogCloud(const FilePath& path_to_pdf,
                                   const string16& print_job_title,
                                   bool modal)
    : browser_(BrowserList::GetLastActive()) {
  Init(path_to_pdf, print_job_title, modal);
}

PrintDialogCloud::~PrintDialogCloud() {
}

void PrintDialogCloud::Init(const FilePath& path_to_pdf,
                            const string16& print_job_title,
                            bool modal) {
  // TODO(scottbyer): Verify GAIA login valid, execute GAIA login if not (should
  // be distilled out of bookmark sync.)
  const int kDefaultWidth = 497;
  const int kDefaultHeight = 332;
  string16 job_title = print_job_title;
  Profile* profile = NULL;
  if (modal) {
    DCHECK(browser_);
    if (job_title.empty() && browser_->GetSelectedTabContents())
      job_title = browser_->GetSelectedTabContents()->GetTitle();
    profile = browser_->GetProfile();
  } else {
    profile = ProfileManager::GetDefaultProfile();
  }
  DCHECK(profile);
  PrefService* pref_service = profile->GetPrefs();
  DCHECK(pref_service);
  if (!pref_service->FindPreference(prefs::kCloudPrintDialogWidth)) {
    pref_service->RegisterIntegerPref(prefs::kCloudPrintDialogWidth,
                                      kDefaultWidth);
  }
  if (!pref_service->FindPreference(prefs::kCloudPrintDialogHeight)) {
    pref_service->RegisterIntegerPref(prefs::kCloudPrintDialogHeight,
                                      kDefaultHeight);
  }

  int width = pref_service->GetInteger(prefs::kCloudPrintDialogWidth);
  int height = pref_service->GetInteger(prefs::kCloudPrintDialogHeight);

  HtmlDialogUIDelegate* dialog_delegate =
      new internal_cloud_print_helpers::CloudPrintHtmlDialogDelegate(
          path_to_pdf, width, height, std::string(), job_title, modal);
  if (modal) {
    DCHECK(browser_);
    browser_->BrowserShowHtmlDialog(dialog_delegate, NULL);
  } else {
    browser::ShowHtmlDialog(NULL, profile, dialog_delegate);
  }
}
