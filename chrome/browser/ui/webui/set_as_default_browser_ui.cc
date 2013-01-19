// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/set_as_default_browser_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/win/win_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/install_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

const char kSetAsDefaultBrowserHistogram[] = "DefaultBrowser.InteractionResult";

// The enum permits registering in UMA the three possible outcomes.
// ACCEPTED: user pressed Next and made Chrome default.
// DECLINED: user simply closed the dialog without making Chrome default.
// REGRETTED: user pressed Next but then elected a different default browser.
// ACCEPTED_IMMERSE: as above with a switch to metro mode.
enum MakeChromeDefaultResult {
  MAKE_CHROME_DEFAULT_ACCEPTED,
  MAKE_CHROME_DEFAULT_DECLINED,
  MAKE_CHROME_DEFAULT_REGRETTED,
  MAKE_CHROME_DEFAULT_ACCEPTED_IMMERSE,
  MAKE_CHROME_DEFAULT_MAX
};

content::WebUIDataSource* CreateSetAsDefaultBrowserUIHTMLSource() {
  content::WebUIDataSource* data_source = ChromeWebUIDataSource::Create(
      chrome::kChromeUIMetroFlowHost);
  data_source->AddLocalizedString("page-title", IDS_METRO_FLOW_TAB_TITLE);
  data_source->AddLocalizedString("flowTitle", IDS_METRO_FLOW_TITLE_SHORT);
  data_source->AddLocalizedString("flowDescription",
                                  IDS_METRO_FLOW_DESCRIPTION);
  data_source->AddLocalizedString("flowNext",
                                  IDS_METRO_FLOW_SET_DEFAULT);
  data_source->AddLocalizedString("chromeLogoString",
                                  IDS_METRO_FLOW_LOGO_STRING_ALT);
  data_source->SetJsonPath("strings.js");
  data_source->AddResourcePath("set_as_default_browser.js",
      IDR_SET_AS_DEFAULT_BROWSER_JS);
  data_source->SetDefaultResource(IDR_SET_AS_DEFAULT_BROWSER_HTML);
  return data_source;
}

// A simple class serving as a delegate for passing down the result of the
// interaction.
class ResponseDelegate {
 public:
  virtual void SetDialogInteractionResult(MakeChromeDefaultResult result) = 0;

 protected:
  virtual ~ResponseDelegate() { }
};

// Event handler for SetAsDefaultBrowserUI. Capable of setting Chrome as the
// default browser on button click, closing itself and triggering Chrome
// restart.
class SetAsDefaultBrowserHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<SetAsDefaultBrowserHandler>,
      public ShellIntegration::DefaultWebClientObserver {
 public:
  explicit SetAsDefaultBrowserHandler(ResponseDelegate* response_delegate);
  virtual ~SetAsDefaultBrowserHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // ShellIntegration::DefaultWebClientObserver implementation.
  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) OVERRIDE;
  virtual void OnSetAsDefaultConcluded(bool close_chrome)  OVERRIDE;
  virtual bool IsInteractiveSetDefaultPermitted() OVERRIDE;

 private:
  // Handler for the 'Next' (or 'make Chrome the Metro browser') button.
  void HandleLaunchSetDefaultBrowserFlow(const ListValue* args);

  // Close this web ui.
  void ConcludeInteraction(MakeChromeDefaultResult interaction_result);

  // Returns true if Chrome should be restarted in immersive mode upon being
  // made the default browser.
  bool ShouldAttemptImmersiveRestart();

  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;
  bool set_default_returned_;
  bool set_default_result_;
  ResponseDelegate* response_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SetAsDefaultBrowserHandler);
};

SetAsDefaultBrowserHandler::SetAsDefaultBrowserHandler(
    ResponseDelegate* response_delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(default_browser_worker_(
          new ShellIntegration::DefaultBrowserWorker(this))),
      set_default_returned_(false), set_default_result_(false),
      response_delegate_(response_delegate) {
}

SetAsDefaultBrowserHandler::~SetAsDefaultBrowserHandler() {
  default_browser_worker_->ObserverDestroyed();
}

void SetAsDefaultBrowserHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "SetAsDefaultBrowser:LaunchSetDefaultBrowserFlow",
      base::Bind(&SetAsDefaultBrowserHandler::HandleLaunchSetDefaultBrowserFlow,
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
    // chrome the default.
    ConcludeInteraction(MAKE_CHROME_DEFAULT_REGRETTED);
  } else if (state == ShellIntegration::STATE_IS_DEFAULT) {
    ConcludeInteraction(ShouldAttemptImmersiveRestart() ?
        MAKE_CHROME_DEFAULT_ACCEPTED_IMMERSE : MAKE_CHROME_DEFAULT_ACCEPTED);
  }

  // Otherwise, keep the dialog open since the user probably didn't make a
  // choice.
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

void SetAsDefaultBrowserHandler::ConcludeInteraction(
    MakeChromeDefaultResult interaction_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (response_delegate_)
    response_delegate_->SetDialogInteractionResult(interaction_result);

  WebContents* contents = web_ui()->GetWebContents();

  if (contents) {
    content::WebContentsDelegate* delegate = contents->GetDelegate();
    if (delegate)
      delegate->CloseContents(contents);
  }
}

bool SetAsDefaultBrowserHandler::ShouldAttemptImmersiveRestart() {
  return (base::win::IsMachineATablet() &&
          !Profile::FromWebUI(web_ui())->GetPrefs()->GetBoolean(
              prefs::kSuppressSwitchToMetroModeOnSetDefault));
}

// A web dialog delegate implementation for when 'Make Chrome Metro' UI
// is displayed on a dialog.
class SetAsDefaultBrowserDialogImpl : public ui::WebDialogDelegate,
                                      public ResponseDelegate {
 public:
  SetAsDefaultBrowserDialogImpl(Profile* profile, Browser* browser);
  virtual ~SetAsDefaultBrowserDialogImpl();
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

  // Overridden from ResponseDelegate:
  virtual void SetDialogInteractionResult(MakeChromeDefaultResult result);

 private:
  // Reset the first-run sentinel file, so must be called on the FILE thread.
  // This is needed if the browser should be restarted in immersive mode.
  // The method is static because the dialog could be destroyed
  // before the task arrives on the FILE thread.
  static void AttemptImmersiveFirstRunRestartOnFileThread();

  Profile* profile_;
  Browser* browser_;
  mutable bool owns_handler_;
  SetAsDefaultBrowserHandler* handler_;
  MakeChromeDefaultResult dialog_interaction_result_;

  DISALLOW_COPY_AND_ASSIGN(SetAsDefaultBrowserDialogImpl);
};

SetAsDefaultBrowserDialogImpl::SetAsDefaultBrowserDialogImpl(Profile* profile,
                                                             Browser* browser)
    : profile_(profile),
      browser_(browser),
      owns_handler_(true),
      handler_(new SetAsDefaultBrowserHandler(this)),
      dialog_interaction_result_(MAKE_CHROME_DEFAULT_DECLINED) {
}

SetAsDefaultBrowserDialogImpl::~SetAsDefaultBrowserDialogImpl() {
  if (owns_handler_)
    delete handler_;
}

void SetAsDefaultBrowserDialogImpl::ShowDialog() {
  // Use a NULL parent window to make sure that the dialog will have an item
  // in the Windows task bar. The code below will make it highlight if the
  // dialog is not in the foreground.
  gfx::NativeWindow native_window = chrome::ShowWebDialog(NULL,
                                                          browser_->profile(),
                                                          this);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      native_window);
  widget->FlashFrame(true);
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
  handlers->push_back(handler_);
  owns_handler_ = false;
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
  // Register the user's response in UMA.
  UMA_HISTOGRAM_ENUMERATION(kSetAsDefaultBrowserHistogram,
                            dialog_interaction_result_,
                            MAKE_CHROME_DEFAULT_MAX);

  if (dialog_interaction_result_ == MAKE_CHROME_DEFAULT_ACCEPTED_IMMERSE) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&SetAsDefaultBrowserDialogImpl::
            AttemptImmersiveFirstRunRestartOnFileThread));
  } else {
    // If the user explicitly elected *not to* make Chrome default, we won't
    // ask again.
    if (dialog_interaction_result_ == MAKE_CHROME_DEFAULT_REGRETTED) {
      PrefService* prefs = profile_->GetPrefs();
      prefs->SetBoolean(prefs::kCheckDefaultBrowser, false);
    }

    // Carry on with a normal chrome session. For the purpose of surfacing this
    // dialog the actual browser window had to remain hidden. Now it's time to
    // show it.
    BrowserWindow* window = browser_->window();
    WebContents* contents = chrome::GetActiveWebContents(browser_);
    window->Show();
    if (contents)
      contents->GetView()->SetInitialFocus();
  }

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

void SetAsDefaultBrowserDialogImpl::SetDialogInteractionResult(
    MakeChromeDefaultResult result) {
  dialog_interaction_result_ = result;
}

void SetAsDefaultBrowserDialogImpl::
    AttemptImmersiveFirstRunRestartOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // If the sentinel was created for this launch, remove it before restarting
  // in immersive mode so that the user is taken through the full first-run
  // flow there.
  if (first_run::IsChromeFirstRun())
    first_run::RemoveSentinel();

  // Do a straight-up restart rather than a mode-switch restart.
  // delegate_execute.exe will choose an immersive launch on the basis of the
  // same IsMachineATablet check, but will not store this as the user's
  // choice.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&browser::AttemptRestart));
}

}  // namespace

SetAsDefaultBrowserUI::SetAsDefaultBrowserUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  ChromeURLDataManager::AddWebUIDataSource(
      Profile::FromWebUI(web_ui), CreateSetAsDefaultBrowserUIHTMLSource());
}

// static
void SetAsDefaultBrowserUI::Show(Profile* profile, Browser* browser) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SetAsDefaultBrowserDialogImpl* dialog =
      new SetAsDefaultBrowserDialogImpl(profile, browser);
  dialog->ShowDialog();
}
