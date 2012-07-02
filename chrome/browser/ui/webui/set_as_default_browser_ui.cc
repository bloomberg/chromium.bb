// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/set_as_default_browser_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/path_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/install_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

ChromeWebUIDataSource* CreateSetAsDefaultBrowserUIHTMLSource() {
  ChromeWebUIDataSource* data_source = new ChromeWebUIDataSource(
      chrome::kChromeUIMetroFlowHost);
  data_source->AddLocalizedString("page-title", IDS_METRO_FLOW_TAB_TITLE);
  data_source->AddLocalizedString("flowTitle", IDS_METRO_FLOW_TITLE_SHORT);
  data_source->AddLocalizedString("flowDescription",
                                  IDS_METRO_FLOW_DESCRIPTION);
  data_source->AddLocalizedString("flowNext",
                                  IDS_METRO_FLOW_SET_DEFAULT);
  data_source->AddLocalizedString("flowCancel",
                                  IDS_METRO_FLOW_SET_DEFAULT_CANCEL);
  data_source->set_json_path("strings.js");
  data_source->add_resource_path("set_as_default_browser.js",
      IDR_SET_AS_DEFAULT_BROWSER_JS);
  data_source->set_default_resource(IDR_SET_AS_DEFAULT_BROWSER_HTML);
  return data_source;
}

// Event handler for SetAsDefaultBrowserUI. Capable of setting Chrome as the
// default browser on button click, closing itself and triggering Chrome
// restart.
class SetAsDefaultBrowserHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<SetAsDefaultBrowserHandler>,
      public ShellIntegration::DefaultWebClientObserver {
 public:
  SetAsDefaultBrowserHandler();
  virtual ~SetAsDefaultBrowserHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // ShellIntegration::DefaultWebClientObserver implementation.
  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) OVERRIDE;
  virtual void OnSetAsDefaultConcluded(bool call_result)  OVERRIDE;
  virtual bool IsInteractiveSetDefaultPermitted() OVERRIDE;

 private:
  // Handler for the 'Next' (or 'make Chrome the Metro browser') button.
  void HandleLaunchSetDefaultBrowserFlow(const ListValue* args);

  // Handler for the 'No, thanks' (Cancel) button.
  void HandleReturnToBrowser(const ListValue* args);

  // Close this web ui.
  void ConcludeInteraction();

  // If required and possible, spawns a new Chrome in Metro mode and closes the
  // current instance. Windows 8 only, on earlier systems it will simply close
  // this UI.
  void ActivateMetroChrome();

  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;
  bool set_default_returned_;
  bool set_default_result_;

  DISALLOW_COPY_AND_ASSIGN(SetAsDefaultBrowserHandler);
};

SetAsDefaultBrowserHandler::SetAsDefaultBrowserHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(default_browser_worker_(
          new ShellIntegration::DefaultBrowserWorker(this))),
      set_default_returned_(false), set_default_result_(false) {
}

SetAsDefaultBrowserHandler::~SetAsDefaultBrowserHandler() {
  default_browser_worker_->ObserverDestroyed();
}

void SetAsDefaultBrowserHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "SetAsDefaultBrowser:LaunchSetDefaultBrowserFlow",
      base::Bind(&SetAsDefaultBrowserHandler::HandleLaunchSetDefaultBrowserFlow,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SetAsDefaultBrowser:ReturnToBrowser",
      base::Bind(&SetAsDefaultBrowserHandler::HandleReturnToBrowser,
                 base::Unretained(this)));
}

void SetAsDefaultBrowserHandler::SetDefaultWebClientUIState(
    ShellIntegration::DefaultWebClientUIState state) {
  // The callback is expected to be invoked once the procedure has completed.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!set_default_returned_)
    return;

  if (state == ShellIntegration::STATE_NOT_DEFAULT && set_default_result_) {
    // The operation concluded, but Chrome is still not the default.
    // If the call has succeeded, this suggests user has decided not to make
    // chrome the default. We fold this UI and move on.
    ConcludeInteraction();
  } else if (state == ShellIntegration::STATE_IS_DEFAULT) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&SetAsDefaultBrowserHandler::ActivateMetroChrome,
                   base::Unretained(this)));
  }
}

void SetAsDefaultBrowserHandler::OnSetAsDefaultConcluded(bool call_result) {
  set_default_returned_ = true;
  set_default_result_ = call_result;
}

bool SetAsDefaultBrowserHandler::IsInteractiveSetDefaultPermitted() {
  return true;
}

void SetAsDefaultBrowserHandler::HandleLaunchSetDefaultBrowserFlow(
    const ListValue* args) {
  set_default_returned_ = false;
  set_default_result_ = false;
  default_browser_worker_->StartSetAsDefault();
}

void SetAsDefaultBrowserHandler::HandleReturnToBrowser(const ListValue* args) {
  ConcludeInteraction();
}

void SetAsDefaultBrowserHandler::ConcludeInteraction() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebContents* contents = web_ui()->GetWebContents();
  if (contents) {
    content::WebContentsDelegate* delegate = contents->GetDelegate();
    if (delegate) {
      if (!delegate->IsPopupOrPanel(contents)) {
        Browser* browser = browser::FindBrowserWithWebContents(contents);
        if (browser)
          chrome::ShowSyncSetup(browser, SyncPromoUI::SOURCE_START_PAGE);
      }
      delegate->CloseContents(contents);
    }
  }
}

void SetAsDefaultBrowserHandler::ActivateMetroChrome() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath cur_chrome_exe;
  bool sentinel_removed = false;
  if (PathService::Get(base::FILE_EXE, &cur_chrome_exe) &&
      first_run::IsChromeFirstRun() &&
      InstallUtil::IsPerUserInstall(cur_chrome_exe.value().c_str())) {
    // If this is per-user install, we will have to remove the sentinel file
    // to assure the user goes through the intended 'first-run flow'.
    sentinel_removed = first_run::RemoveSentinel();
  }

  if (ShellIntegration::ActivateMetroChrome()) {
    // If Metro Chrome has been activated, we should close this process.
    // We are restarting as metro now.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&BrowserList::CloseAllBrowsersWithProfile,
                   Profile::FromWebUI(web_ui())));
  } else {
    // This will return false if the operation failed for any reason,
    // including invocation under a Windows version earlier than 8.
    // In such case we simply close the window and carry on.
    if (sentinel_removed)
      first_run::CreateSentinel();

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SetAsDefaultBrowserHandler::ConcludeInteraction,
                    base::Unretained(this)));
  }
}

// A web dialog delegate implementation for when 'Make Chrome Metro' UI
// is displayed on a dialog.
class SetAsDefaultBrowserDialogImpl : public ui::WebDialogDelegate {
 public:
  SetAsDefaultBrowserDialogImpl(Profile* profile, Browser* browser);
  // Show a modal web dialog with kChromeUIMetroFlowURL page.
  void ShowDialog();

 protected:
  // Overridden from WebDialogDelegate:
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(WebContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;

 private:
  Profile* profile_;
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(SetAsDefaultBrowserDialogImpl);
};

SetAsDefaultBrowserDialogImpl::SetAsDefaultBrowserDialogImpl(Profile* profile,
                                                             Browser* browser)
    : profile_(profile), browser_(browser) {
}

void SetAsDefaultBrowserDialogImpl::ShowDialog() {
  chrome::ShowWebDialog(browser_->window()->GetNativeWindow(),
                        browser_->profile(), this);
}

ui::ModalType SetAsDefaultBrowserDialogImpl::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

string16 SetAsDefaultBrowserDialogImpl::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_METRO_FLOW_TAB_TITLE);
}

GURL SetAsDefaultBrowserDialogImpl::GetDialogContentURL() const {
  std::string url_string(chrome::kChromeUIMetroFlowURL);
  return GURL(url_string);
}

void SetAsDefaultBrowserDialogImpl::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void SetAsDefaultBrowserDialogImpl::GetDialogSize(gfx::Size* size) const {
  PrefService* prefs = profile_->GetPrefs();
  gfx::Font approximate_web_font(
      prefs->GetString(prefs::kWebKitSansSerifFontFamily),
      prefs->GetInteger(prefs::kWebKitDefaultFontSize));

  *size = ui::GetLocalizedContentsSizeForFont(
      IDS_METRO_FLOW_WIDTH_CHARS, IDS_METRO_FLOW_HEIGHT_LINES,
      approximate_web_font);
}

std::string SetAsDefaultBrowserDialogImpl::GetDialogArgs() const {
  return "[]";
}

void SetAsDefaultBrowserDialogImpl::OnDialogClosed(
    const std::string& json_retval) {
  delete this;
}

void SetAsDefaultBrowserDialogImpl::OnCloseContents(WebContents* source,
                                                    bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool SetAsDefaultBrowserDialogImpl::ShouldShowDialogTitle() const {
  return true;
}

bool SetAsDefaultBrowserDialogImpl::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return true;
}

}  // namespace

SetAsDefaultBrowserUI::SetAsDefaultBrowserUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new SetAsDefaultBrowserHandler());
  ChromeURLDataManager::AddDataSource(Profile::FromWebUI(web_ui),
                                      CreateSetAsDefaultBrowserUIHTMLSource());
}

// static
void SetAsDefaultBrowserUI::Show(Profile* profile,
                                 Browser* browser,
                                 bool dialog) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (dialog) {
    SetAsDefaultBrowserDialogImpl* dialog =
        new SetAsDefaultBrowserDialogImpl(profile, browser);
    dialog->ShowDialog();
  } else {
    GURL url(chrome::kChromeUIMetroFlowURL);
    chrome::NavigateParams params(
        chrome::GetSingletonTabNavigateParams(browser, url));
    params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
    chrome::ShowSingletonTabOverwritingNTP(browser, params);
  }
}
