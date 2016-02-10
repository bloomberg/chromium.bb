// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/set_as_default_browser_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/installer/util/install_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
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

// The enum permits registering in UMA the three possible outcomes (do not
// reorder these).
// ACCEPTED: user pressed Next and made Chrome default.
// DECLINED: user simply closed the dialog without making Chrome default.
// REGRETTED: user pressed Next but then elected a different default browser.
enum MakeChromeDefaultResult {
  MAKE_CHROME_DEFAULT_ACCEPTED = 0,
  MAKE_CHROME_DEFAULT_DECLINED = 1,
  MAKE_CHROME_DEFAULT_REGRETTED = 2,
  // MAKE_CHROME_DEFAULT_ACCEPTED_IMMERSE = 3,  // Deprecated.
  MAKE_CHROME_DEFAULT_MAX
};

content::WebUIDataSource* CreateSetAsDefaultBrowserUIHTMLSource() {
  content::WebUIDataSource* data_source = content::WebUIDataSource::Create(
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
      public shell_integration::DefaultWebClientObserver {
 public:
  explicit SetAsDefaultBrowserHandler(
      const base::WeakPtr<ResponseDelegate>& response_delegate);
  ~SetAsDefaultBrowserHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // shell_integration::DefaultWebClientObserver implementation.
  void SetDefaultWebClientUIState(
      shell_integration::DefaultWebClientUIState state) override;
  void OnSetAsDefaultConcluded(bool close_chrome) override;
  bool IsInteractiveSetDefaultPermitted() override;

 private:
  // Handler for the 'Next' (or 'make Chrome the Metro browser') button.
  void HandleLaunchSetDefaultBrowserFlow(const base::ListValue* args);

  // Close this web ui.
  void ConcludeInteraction(MakeChromeDefaultResult interaction_result);

  scoped_refptr<shell_integration::DefaultBrowserWorker>
      default_browser_worker_;
  bool set_default_returned_;
  bool set_default_result_;
  base::WeakPtr<ResponseDelegate> response_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SetAsDefaultBrowserHandler);
};

SetAsDefaultBrowserHandler::SetAsDefaultBrowserHandler(
    const base::WeakPtr<ResponseDelegate>& response_delegate)
    : default_browser_worker_(
          new shell_integration::DefaultBrowserWorker(this)),
      set_default_returned_(false),
      set_default_result_(false),
      response_delegate_(response_delegate) {}

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
    shell_integration::DefaultWebClientUIState state) {
  // The callback is expected to be invoked once the procedure has completed.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!set_default_returned_)
    return;

  if (state == shell_integration::STATE_NOT_DEFAULT && set_default_result_) {
    // The operation concluded, but Chrome is still not the default.
    // If the call has succeeded, this suggests user has decided not to make
    // chrome the default.
    ConcludeInteraction(MAKE_CHROME_DEFAULT_REGRETTED);
  } else if (state == shell_integration::STATE_IS_DEFAULT) {
    ConcludeInteraction(MAKE_CHROME_DEFAULT_ACCEPTED);
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
    const base::ListValue* args) {
  set_default_returned_ = false;
  set_default_result_ = false;
  default_browser_worker_->StartSetAsDefault();
}

void SetAsDefaultBrowserHandler::ConcludeInteraction(
    MakeChromeDefaultResult interaction_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (response_delegate_)
    response_delegate_->SetDialogInteractionResult(interaction_result);

  WebContents* contents = web_ui()->GetWebContents();

  if (contents) {
    content::WebContentsDelegate* delegate = contents->GetDelegate();
    if (delegate)
      delegate->CloseContents(contents);
  }
}

// A web dialog delegate implementation for when 'Make Chrome Metro' UI
// is displayed on a dialog.
class SetAsDefaultBrowserDialogImpl : public ui::WebDialogDelegate,
                                      public ResponseDelegate,
                                      public chrome::BrowserListObserver {
 public:
  SetAsDefaultBrowserDialogImpl(Profile* profile, Browser* browser);
  ~SetAsDefaultBrowserDialogImpl() override;
  // Show a modal web dialog with kChromeUIMetroFlowURL page.
  void ShowDialog();

 protected:
  // Overridden from WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(WebContents* source, bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool HandleContextMenu(const content::ContextMenuParams& params) override;

  // Overridden from ResponseDelegate:
  void SetDialogInteractionResult(MakeChromeDefaultResult result) override;

  // Overridden from BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

 private:
  Profile* profile_;
  Browser* browser_;
  mutable bool owns_handler_;
  base::WeakPtrFactory<ResponseDelegate> response_delegate_ptr_factory_;
  SetAsDefaultBrowserHandler* handler_;
  MakeChromeDefaultResult dialog_interaction_result_;

  DISALLOW_COPY_AND_ASSIGN(SetAsDefaultBrowserDialogImpl);
};

SetAsDefaultBrowserDialogImpl::SetAsDefaultBrowserDialogImpl(Profile* profile,
                                                             Browser* browser)
    : profile_(profile),
      browser_(browser),
      owns_handler_(true),
      response_delegate_ptr_factory_(this),
      handler_(new SetAsDefaultBrowserHandler(
          response_delegate_ptr_factory_.GetWeakPtr())),
      dialog_interaction_result_(MAKE_CHROME_DEFAULT_DECLINED) {
  BrowserList::AddObserver(this);
}

SetAsDefaultBrowserDialogImpl::~SetAsDefaultBrowserDialogImpl() {
  if (browser_)
    BrowserList::RemoveObserver(this);
  if (owns_handler_)
    delete handler_;
}

void SetAsDefaultBrowserDialogImpl::ShowDialog() {
  // Use a NULL parent window to make sure that the dialog will have an item
  // in the Windows task bar. The code below will make it highlight if the
  // dialog is not in the foreground.
  gfx::NativeWindow native_window = chrome::ShowWebDialog(NULL, profile_, this);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      native_window);
  widget->FlashFrame(true);
}

ui::ModalType SetAsDefaultBrowserDialogImpl::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 SetAsDefaultBrowserDialogImpl::GetDialogTitle() const {
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

  // If the user explicitly elected *not to* make Chrome default, we won't
  // ask again.
  if (dialog_interaction_result_ == MAKE_CHROME_DEFAULT_REGRETTED) {
    PrefService* prefs = profile_->GetPrefs();
    prefs->SetBoolean(prefs::kCheckDefaultBrowser, false);
  }

  // Carry on with a normal chrome session. For the purpose of surfacing this
  // dialog the actual browser window had to remain hidden. Now it's time to
  // show it.
  if (browser_) {
    BrowserWindow* window = browser_->window();
    WebContents* contents = browser_->tab_strip_model()->GetActiveWebContents();
    window->Show();
    if (contents)
      contents->SetInitialFocus();
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

void SetAsDefaultBrowserDialogImpl::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser) {
    browser_ = NULL;
    BrowserList::RemoveObserver(this);
  }
}

}  // namespace

SetAsDefaultBrowserUI::SetAsDefaultBrowserUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  content::WebUIDataSource::Add(
      Profile::FromWebUI(web_ui), CreateSetAsDefaultBrowserUIHTMLSource());
}

// static
void SetAsDefaultBrowserUI::Show(Profile* profile, Browser* browser) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SetAsDefaultBrowserDialogImpl* dialog =
      new SetAsDefaultBrowserDialogImpl(profile, browser);
  dialog->ShowDialog();
}
