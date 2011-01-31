// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"

#include "base/json/json_writer.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/web_ui_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_message_handler.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_source.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/service/service_process_control_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_messages.h"
#include "gfx/font.h"
#include "grit/chromium_strings.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_font_util.h"

#if defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/html_dialog_gtk.h"
#endif  // defined(TOOLKIT_GTK)

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/browser_dialogs.h"
#endif  // defined(TOOLKIT_GTK)

static const wchar_t kGaiaLoginIFrameXPath[] = L"//iframe[@id='gaialogin']";
static const wchar_t kDoneIframeXPath[] = L"//iframe[@id='setupdone']";

////////////////////////////////////////////////////////////////////////////////
// CloudPrintSetupFlow implementation.

// static
CloudPrintSetupFlow* CloudPrintSetupFlow::OpenDialog(
    Profile* profile, Delegate* delegate, gfx::NativeWindow parent_window) {
  // Set the arguments for showing the gaia login page.
  DictionaryValue args;
  args.SetString("user", "");
  args.SetInteger("error", 0);
  args.SetBoolean("editable_user", true);

  bool setup_done = false;
  if (profile->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail) &&
      !profile->GetPrefs()->GetString(prefs::kCloudPrintEmail).empty())
    setup_done = true;
  args.SetString("pageToShow", setup_done ? "setupdone" : "cloudprintsetup");

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  CloudPrintSetupFlow* flow = new CloudPrintSetupFlow(json_args, profile,
                                                      delegate, setup_done);
  // We may not always have a browser. This can happen when we are being
  // invoked in the context of a "token expired" notfication. If we don't have
  // a brower, use the underlying dialog system to show the dialog without
  // using a browser.
  Browser* browser = BrowserList::GetLastActive();
  if (browser) {
    browser->BrowserShowHtmlDialog(flow, parent_window);
  } else {
#if defined(TOOLKIT_VIEWS)
    browser::ShowHtmlDialogView(parent_window, profile, flow);
#elif defined(TOOLKIT_GTK)
    HtmlDialogGtk* html_dialog =
        new HtmlDialogGtk(profile, flow, parent_window);
    html_dialog->InitDialog();
#endif  // defined(TOOLKIT_VIEWS)
  // TODO(sanjeevr): Implement the "no browser" scenario for the Mac.
  }
  return flow;
}

CloudPrintSetupFlow::CloudPrintSetupFlow(const std::string& args,
                                         Profile* profile,
                                         Delegate* delegate,
                                         bool setup_done)
    : dom_ui_(NULL),
      dialog_start_args_(args),
      setup_done_(setup_done),
      process_control_(NULL),
      delegate_(delegate) {
  // TODO(hclam): The data source should be added once.
  profile_ = profile;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(ChromeURLDataManager::GetInstance(),
                        &ChromeURLDataManager::AddDataSource,
                        make_scoped_refptr(new CloudPrintSetupSource())));
}

CloudPrintSetupFlow::~CloudPrintSetupFlow() {
}

void CloudPrintSetupFlow::Focus() {
  // TODO(pranavk): implement this method.
  NOTIMPLEMENTED();
}

///////////////////////////////////////////////////////////////////////////////
// HtmlDialogUIDelegate implementation.
GURL CloudPrintSetupFlow::GetDialogContentURL() const {
  return GURL("chrome://cloudprintsetup/setupflow");
}

void CloudPrintSetupFlow::GetDOMMessageHandlers(
    std::vector<DOMMessageHandler*>* handlers) const {
  // Create the message handler only after we are asked, the caller is
  // responsible for deleting the objects.
  handlers->push_back(
      new CloudPrintSetupMessageHandler(
          const_cast<CloudPrintSetupFlow*>(this)));
}

void CloudPrintSetupFlow::GetDialogSize(gfx::Size* size) const {
  PrefService* prefs = profile_->GetPrefs();
  gfx::Font approximate_web_font(
      UTF8ToUTF16(prefs->GetString(prefs::kWebKitSansSerifFontFamily)),
      prefs->GetInteger(prefs::kWebKitDefaultFontSize));

  if (setup_done_) {
    *size = ui::GetLocalizedContentsSizeForFont(
        IDS_CLOUD_PRINT_SETUP_WIZARD_DONE_WIDTH_CHARS,
        IDS_CLOUD_PRINT_SETUP_WIZARD_DONE_HEIGHT_LINES,
        approximate_web_font);
  } else {
    *size = ui::GetLocalizedContentsSizeForFont(
        IDS_CLOUD_PRINT_SETUP_WIZARD_WIDTH_CHARS,
        IDS_CLOUD_PRINT_SETUP_WIZARD_HEIGHT_LINES,
        approximate_web_font);
  }
}

// A callback to notify the delegate that the dialog closed.
void CloudPrintSetupFlow::OnDialogClosed(const std::string& json_retval) {
  // If we are fetching the token then cancel the request.
  if (authenticator_.get())
    authenticator_->CancelRequest();

  if (delegate_) {
    delegate_->OnDialogClosed();
  }
  delete this;
}

std::string CloudPrintSetupFlow::GetDialogArgs() const {
    return dialog_start_args_;
}

void CloudPrintSetupFlow::OnCloseContents(TabContents* source,
                                          bool* out_close_dialog) {
}

std::wstring CloudPrintSetupFlow::GetDialogTitle() const {
  return UTF16ToWideHack(
      l10n_util::GetStringUTF16(IDS_CLOUD_PRINT_SETUP_DIALOG_TITLE));
}

bool CloudPrintSetupFlow::IsDialogModal() const {
  // We are always modeless.
  return false;
}

bool CloudPrintSetupFlow::ShouldShowDialogTitle() const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// GaiaAuthConsumer implementation.
void CloudPrintSetupFlow::OnClientLoginFailure(
    const GoogleServiceAuthError& error) {
  ShowGaiaFailed(error);
  authenticator_.reset();
}

void CloudPrintSetupFlow::OnClientLoginSuccess(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  // Save the token for the cloud print proxy.
  lsid_ = credentials.lsid;

  // Show that Gaia login has succeeded.
  ShowGaiaSuccessAndSettingUp();
  authenticator_.reset();

  profile_->GetCloudPrintProxyService()->EnableForUser(credentials.lsid,
                                                       login_);
  // TODO(sanjeevr): Should we wait and verify that the enable succeeded?
  ShowSetupDone();
}

///////////////////////////////////////////////////////////////////////////////
// Methods called by CloudPrintSetupMessageHandler
void CloudPrintSetupFlow::Attach(DOMUI* dom_ui) {
  dom_ui_ = dom_ui;
}

void CloudPrintSetupFlow::OnUserSubmittedAuth(const std::string& user,
                                              const std::string& password,
                                              const std::string& captcha) {
  // Save the login name only.
  login_ = user;

  // Start the authenticator.
  authenticator_.reset(
      new GaiaAuthFetcher(this, GaiaConstants::kChromeSource,
                          profile_->GetRequestContext()));

  authenticator_->StartClientLogin(user, password,
                                   GaiaConstants::kCloudPrintService,
                                   "", captcha,
                                   GaiaAuthFetcher::HostedAccountsAllowed);
}

void CloudPrintSetupFlow::OnUserClickedLearnMore() {
  dom_ui_->tab_contents()->OpenURL(CloudPrintURL::GetCloudPrintLearnMoreURL(),
                                   GURL(), NEW_FOREGROUND_TAB,
                                   PageTransition::LINK);
}

void CloudPrintSetupFlow::OnUserClickedPrintTestPage() {
  dom_ui_->tab_contents()->OpenURL(CloudPrintURL::GetCloudPrintTestPageURL(),
                                   GURL(), NEW_FOREGROUND_TAB,
                                   PageTransition::LINK);
}

///////////////////////////////////////////////////////////////////////////////
// Helper methods for showing contents of the DOM UI
void CloudPrintSetupFlow::ShowGaiaLogin(const DictionaryValue& args) {
  if (dom_ui_)
    dom_ui_->CallJavascriptFunction(L"cloudprint.showSetupLogin");

  std::string json;
  base::JSONWriter::Write(&args, false, &json);
  std::wstring javascript = std::wstring(L"showGaiaLogin") +
      L"(" + UTF8ToWide(json) + L");";
  ExecuteJavascriptInIFrame(kGaiaLoginIFrameXPath, javascript);
}

void CloudPrintSetupFlow::ShowGaiaSuccessAndSettingUp() {
  ExecuteJavascriptInIFrame(kGaiaLoginIFrameXPath,
                            L"showGaiaSuccessAndSettingUp();");
}

void CloudPrintSetupFlow::ShowGaiaFailed(const GoogleServiceAuthError& error) {
  DictionaryValue args;
  args.SetString("pageToShow", "cloudprintsetup");
  args.SetString("user", "");
  args.SetInteger("error", error.state());
  args.SetBoolean("editable_user", true);
  args.SetString("captchaUrl", error.captcha().image_url.spec());
  ShowGaiaLogin(args);
}

void CloudPrintSetupFlow::ShowSetupDone() {
  setup_done_ = true;
  string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  std::wstring message =
      UTF16ToWideHack(l10n_util::GetStringFUTF16(IDS_CLOUD_PRINT_SETUP_DONE,
                                                 product_name,
                                                 UTF8ToUTF16(login_)));
  std::wstring javascript = L"cloudprint.setMessage('" + message + L"');";
  ExecuteJavascriptInIFrame(kDoneIframeXPath, javascript);

  if (dom_ui_) {
    PrefService* prefs = profile_->GetPrefs();
    gfx::Font approximate_web_font(
        UTF8ToUTF16(prefs->GetString(prefs::kWebKitSansSerifFontFamily)),
        prefs->GetInteger(prefs::kWebKitDefaultFontSize));
    gfx::Size done_size = ui::GetLocalizedContentsSizeForFont(
        IDS_CLOUD_PRINT_SETUP_WIZARD_DONE_WIDTH_CHARS,
        IDS_CLOUD_PRINT_SETUP_WIZARD_DONE_HEIGHT_LINES,
        approximate_web_font);

    FundamentalValue new_width(done_size.width());
    FundamentalValue new_height(done_size.height());
    dom_ui_->CallJavascriptFunction(L"cloudprint.showSetupDone",
                                    new_width, new_height);
  }

  ExecuteJavascriptInIFrame(kDoneIframeXPath, L"cloudprint.onPageShown();");
}

void CloudPrintSetupFlow::ExecuteJavascriptInIFrame(
    const std::wstring& iframe_xpath,
    const std::wstring& js) {
  if (dom_ui_) {
    RenderViewHost* rvh = dom_ui_->tab_contents()->render_view_host();
    rvh->ExecuteJavascriptInWebFrame(iframe_xpath, js);
  }
}
