// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_dialog_controller.h"

#include <algorithm>
#include <string>

#include "base/auto_reset.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_web_contents_observer.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/webplugininfo.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

using content::NativeWebKeyboardEvent;
using content::NavigationController;
using content::RenderProcessHost;
using content::WebContents;
using content::WebUIMessageHandler;
using ui::WebDialogDelegate;
using ui::WebDialogWebContentsDelegate;

namespace {

void EnableInternalPDFPluginForContents(WebContents* preview_dialog) {
  // Always enable the internal PDF plugin for the print preview page.
  base::FilePath pdf_plugin_path;
  if (!PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_plugin_path))
    return;

  content::WebPluginInfo pdf_plugin;
  if (!content::PluginService::GetInstance()->GetPluginInfoByPath(
      pdf_plugin_path, &pdf_plugin))
    return;

  ChromePluginServiceFilter::GetInstance()->OverridePluginForFrame(
      preview_dialog->GetRenderProcessHost()->GetID(),
      preview_dialog->GetMainFrame()->GetRoutingID(),
      GURL(), pdf_plugin);
}

// WebDialogDelegate that specifies what the print preview dialog
// will look like.
class PrintPreviewDialogDelegate : public WebDialogDelegate {
 public:
  explicit PrintPreviewDialogDelegate(WebContents* initiator);
  virtual ~PrintPreviewDialogDelegate();

  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual base::string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(WebContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;

 private:
  WebContents* initiator_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDialogDelegate);
};

PrintPreviewDialogDelegate::PrintPreviewDialogDelegate(WebContents* initiator)
    : initiator_(initiator) {
}

PrintPreviewDialogDelegate::~PrintPreviewDialogDelegate() {
}

ui::ModalType PrintPreviewDialogDelegate::GetDialogModalType() const {
  // Not used, returning dummy value.
  NOTREACHED();
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 PrintPreviewDialogDelegate::GetDialogTitle() const {
  // Only used on Windows? UI folks prefer no title.
  return base::string16();
}

GURL PrintPreviewDialogDelegate::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIPrintURL);
}

void PrintPreviewDialogDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* /* handlers */) const {
  // PrintPreviewUI adds its own message handlers.
}

void PrintPreviewDialogDelegate::GetDialogSize(gfx::Size* size) const {
  DCHECK(size);
  const gfx::Size kMinDialogSize(800, 480);
  const int kBorder = 25;
  *size = kMinDialogSize;

  web_modal::WebContentsModalDialogHost* host = NULL;
  Browser* browser = chrome::FindBrowserWithWebContents(initiator_);
  if (browser)
    host = browser->window()->GetWebContentsModalDialogHost();

  if (host) {
    size->SetToMax(host->GetMaximumDialogSize());
    size->Enlarge(-2 * kBorder, -kBorder);
  } else {
    size->SetToMax(initiator_->GetView()->GetContainerSize());
    size->Enlarge(-2 * kBorder, -2 * kBorder);
  }

#if defined(OS_MACOSX)
  // Limit the maximum size on MacOS X.
  // http://crbug.com/105815
  const gfx::Size kMaxDialogSize(1000, 660);
  size->SetToMin(kMaxDialogSize);
#endif
}

std::string PrintPreviewDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void PrintPreviewDialogDelegate::OnDialogClosed(
    const std::string& /* json_retval */) {
}

void PrintPreviewDialogDelegate::OnCloseContents(WebContents* /* source */,
                                                 bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool PrintPreviewDialogDelegate::ShouldShowDialogTitle() const {
  return false;
}

// WebContentsDelegate that forwards shortcut keys in the print preview
// renderer to the browser.
class PrintPreviewWebContentDelegate : public WebDialogWebContentsDelegate {
 public:
  PrintPreviewWebContentDelegate(Profile* profile, WebContents* initiator);
  virtual ~PrintPreviewWebContentDelegate();

  // Overridden from WebDialogWebContentsDelegate:
  virtual void HandleKeyboardEvent(
      WebContents* source,
      const NativeWebKeyboardEvent& event) OVERRIDE;

 private:
  WebContents* initiator_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewWebContentDelegate);
};

PrintPreviewWebContentDelegate::PrintPreviewWebContentDelegate(
    Profile* profile,
    WebContents* initiator)
    : WebDialogWebContentsDelegate(profile, new ChromeWebContentsHandler),
      initiator_(initiator) {}

PrintPreviewWebContentDelegate::~PrintPreviewWebContentDelegate() {}

void PrintPreviewWebContentDelegate::HandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event) {
  // Disabled on Mac due to http://crbug.com/112173
#if !defined(OS_MACOSX)
  Browser* current_browser = chrome::FindBrowserWithWebContents(initiator_);
  if (!current_browser)
    return;
  current_browser->window()->HandleKeyboardEvent(event);
#endif
}

}  // namespace

namespace printing {

struct PrintPreviewDialogController::Operation {
  class Observer : public content::WebContentsObserver,
                   public content::RenderProcessHostObserver {
   public:
    Observer();
    virtual ~Observer();

    void StartObserving(PrintPreviewDialogController* owner,
                        WebContents* web_contents);
    void StopObserving();

   private:
    // content::WebContentsObserver
    virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;
    virtual void NavigationEntryCommitted(
        const content::LoadCommittedDetails& load_details) OVERRIDE;
    // content::RenderProcessHostObserver
    virtual void RenderProcessExited(RenderProcessHost* host,
                                     base::ProcessHandle handle,
                                     base::TerminationStatus status,
                                     int exit_code) OVERRIDE;
    virtual void RenderProcessHostDestroyed(RenderProcessHost* host) OVERRIDE;

    PrintPreviewDialogController* owner_;
    RenderProcessHost* host_;
  };

  Operation();

  WebContents* preview_dialog;
  WebContents* initiator;
  Observer preview_dialog_observer;
  Observer initiator_observer;

  DISALLOW_COPY_AND_ASSIGN(Operation);
};

PrintPreviewDialogController::Operation::Operation() : preview_dialog(NULL),
                                                       initiator(NULL) {
}

PrintPreviewDialogController::Operation::Observer::Observer() : owner_(NULL),
                                                                host_(NULL) {
}

PrintPreviewDialogController::Operation::Observer::~Observer() {
  StopObserving();
}

void PrintPreviewDialogController::Operation::Observer::StartObserving(
    PrintPreviewDialogController* owner,
    WebContents* web_contents) {
  owner_ = owner;
  Observe(web_contents);
  host_ = web_contents->GetRenderProcessHost();
  host_->AddObserver(this);
}

void PrintPreviewDialogController::Operation::Observer::StopObserving() {
  Observe(NULL);
  if (host_) {
    host_->RemoveObserver(this);
    host_ = NULL;
  }
}

void PrintPreviewDialogController::Operation::Observer::WebContentsDestroyed(
    WebContents* web_contents) {
  owner_->OnWebContentsDestroyed(web_contents);
}

void
PrintPreviewDialogController::Operation::Observer::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  owner_->OnNavigationEntryCommitted(web_contents(), &load_details);
}

void PrintPreviewDialogController::Operation::Observer::RenderProcessExited(
    RenderProcessHost* host,
    base::ProcessHandle handle,
    base::TerminationStatus status,
    int exit_code) {
  owner_->OnRenderProcessExited(host);
}

void
PrintPreviewDialogController::Operation::Observer::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  host_ = NULL;
}

PrintPreviewDialogController::PrintPreviewDialogController()
    : waiting_for_new_preview_page_(false),
      is_creating_print_preview_dialog_(false) {
}

// static
PrintPreviewDialogController* PrintPreviewDialogController::GetInstance() {
  if (!g_browser_process)
    return NULL;
  return g_browser_process->print_preview_dialog_controller();
}

// static
void PrintPreviewDialogController::PrintPreview(WebContents* initiator) {
  if (initiator->ShowingInterstitialPage())
    return;

  PrintPreviewDialogController* dialog_controller = GetInstance();
  if (!dialog_controller)
    return;
  if (!dialog_controller->GetOrCreatePreviewDialog(initiator))
    PrintViewManager::FromWebContents(initiator)->PrintPreviewDone();
}

WebContents* PrintPreviewDialogController::GetOrCreatePreviewDialog(
    WebContents* initiator) {
  DCHECK(initiator);

  // Get the print preview dialog for |initiator|.
  WebContents* preview_dialog = GetPrintPreviewForContents(initiator);
  if (!preview_dialog)
    return CreatePrintPreviewDialog(initiator);

  // Show the initiator holding the existing preview dialog.
  initiator->GetDelegate()->ActivateContents(initiator);
  return preview_dialog;
}

WebContents* PrintPreviewDialogController::GetPrintPreviewForContents(
    WebContents* contents) const {
  for (size_t i = 0; i < preview_operations_.size(); ++i) {
    Operation* operation = preview_operations_[i];
    if (operation->preview_dialog == contents ||
        operation->initiator == contents) {
      return operation->preview_dialog;
    }
  }
  return NULL;
}

WebContents* PrintPreviewDialogController::GetInitiator(
    WebContents* preview_dialog) {
  for (size_t i = 0; i < preview_operations_.size(); ++i) {
    Operation* operation = preview_operations_[i];
    if (operation->preview_dialog == preview_dialog)
      return operation->initiator;
  }
  return NULL;
}

// static
bool PrintPreviewDialogController::IsPrintPreviewDialog(WebContents* contents) {
  return IsPrintPreviewURL(contents->GetURL());
}

// static
bool PrintPreviewDialogController::IsPrintPreviewURL(const GURL& url) {
  return (url.SchemeIs(content::kChromeUIScheme) &&
          url.host() == chrome::kChromeUIPrintHost);
}

void PrintPreviewDialogController::EraseInitiatorInfo(
    WebContents* preview_dialog) {
  for (size_t i = 0; i < preview_operations_.size(); ++i) {
    Operation* operation = preview_operations_[i];
    if (operation->preview_dialog == preview_dialog) {
      operation->initiator_observer.StopObserving();
      operation->initiator = NULL;
      return;
    }
  }
}

PrintPreviewDialogController::~PrintPreviewDialogController() {
  DCHECK_EQ(0U, preview_operations_.size());
}

void PrintPreviewDialogController::OnRenderProcessExited(
    RenderProcessHost* rph) {
  // Store contents in a vector and deal with them after iterating through
  // |preview_operations_| because RemoveFoo() can change |preview_operations_|.
  std::vector<WebContents*> closed_initiators;
  std::vector<WebContents*> closed_preview_dialogs;
  for (size_t i = 0; i < preview_operations_.size(); ++i) {
    Operation* operation = preview_operations_[i];
    WebContents* preview_dialog = operation->preview_dialog;
    WebContents* initiator = operation->initiator;
    if (preview_dialog->GetRenderProcessHost() == rph) {
      closed_preview_dialogs.push_back(preview_dialog);
    } else if (initiator &&
               initiator->GetRenderProcessHost() == rph) {
      closed_initiators.push_back(initiator);
    }
  }

  for (size_t i = 0; i < closed_preview_dialogs.size(); ++i) {
    RemovePreviewDialog(closed_preview_dialogs[i]);
    if (content::WebUI* web_ui = closed_preview_dialogs[i]->GetWebUI()) {
      PrintPreviewUI* print_preview_ui =
          static_cast<PrintPreviewUI*>(web_ui->GetController());
      if (print_preview_ui)
        print_preview_ui->OnPrintPreviewDialogClosed();
    }
  }

  for (size_t i = 0; i < closed_initiators.size(); ++i)
    RemoveInitiator(closed_initiators[i]);
}

void PrintPreviewDialogController::OnWebContentsDestroyed(
    WebContents* contents) {
  WebContents* preview_dialog = GetPrintPreviewForContents(contents);
  if (!preview_dialog) {
    NOTREACHED();
    return;
  }

  if (contents == preview_dialog)
    RemovePreviewDialog(contents);
  else
    RemoveInitiator(contents);
}

void PrintPreviewDialogController::OnNavigationEntryCommitted(
    WebContents* contents, const content::LoadCommittedDetails* details) {
  WebContents* preview_dialog = GetPrintPreviewForContents(contents);
  if (!preview_dialog) {
    NOTREACHED();
    return;
  }

  if (contents == preview_dialog) {
    // Preview dialog navigated.
    if (details) {
      content::PageTransition transition_type =
          details->entry->GetTransitionType();
      content::NavigationType nav_type = details->type;

      // New |preview_dialog| is created. Don't update/erase map entry.
      if (waiting_for_new_preview_page_ &&
          transition_type == content::PAGE_TRANSITION_AUTO_TOPLEVEL &&
          nav_type == content::NAVIGATION_TYPE_NEW_PAGE) {
        waiting_for_new_preview_page_ = false;
        SaveInitiatorTitle(preview_dialog);
        return;
      }

      // Cloud print sign-in causes a reload.
      if (!waiting_for_new_preview_page_ &&
          transition_type == content::PAGE_TRANSITION_RELOAD &&
          nav_type == content::NAVIGATION_TYPE_EXISTING_PAGE &&
          IsPrintPreviewURL(details->previous_url)) {
        return;
      }
    }
    NOTREACHED();
    return;
  }

  RemoveInitiator(contents);
}

WebContents* PrintPreviewDialogController::CreatePrintPreviewDialog(
    WebContents* initiator) {
  base::AutoReset<bool> auto_reset(&is_creating_print_preview_dialog_, true);
  Profile* profile =
      Profile::FromBrowserContext(initiator->GetBrowserContext());

  // |web_dialog_ui_delegate| deletes itself in
  // PrintPreviewDialogDelegate::OnDialogClosed().
  WebDialogDelegate* web_dialog_delegate =
      new PrintPreviewDialogDelegate(initiator);
  // |web_dialog_delegate|'s owner is |constrained_delegate|.
  PrintPreviewWebContentDelegate* pp_wcd =
      new PrintPreviewWebContentDelegate(profile, initiator);
  ConstrainedWebDialogDelegate* constrained_delegate =
      CreateConstrainedWebDialog(profile,
                                 web_dialog_delegate,
                                 pp_wcd,
                                 initiator);
  WebContents* preview_dialog = constrained_delegate->GetWebContents();
  EnableInternalPDFPluginForContents(preview_dialog);
  PrintViewManager::CreateForWebContents(preview_dialog);
  extensions::ExtensionWebContentsObserver::CreateForWebContents(
      preview_dialog);

  waiting_for_new_preview_page_ = true;

  // Add an entry to the map.
  Operation* operation = new Operation;
  operation->preview_dialog = preview_dialog;
  operation->initiator = initiator;
  operation->preview_dialog_observer.StartObserving(this, preview_dialog);
  operation->initiator_observer.StartObserving(this, initiator);
  preview_operations_.push_back(operation);

  return preview_dialog;
}

void PrintPreviewDialogController::SaveInitiatorTitle(
    WebContents* preview_dialog) {
  WebContents* initiator = GetInitiator(preview_dialog);
  if (initiator && preview_dialog->GetWebUI()) {
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
        preview_dialog->GetWebUI()->GetController());
    print_preview_ui->SetInitiatorTitle(
        PrintViewManager::FromWebContents(initiator)->RenderSourceName());
  }
}

void PrintPreviewDialogController::RemoveInitiator(
    WebContents* initiator) {
  WebContents* preview_dialog = GetPrintPreviewForContents(initiator);
  DCHECK(preview_dialog);

  // Update the map entry first, so when the print preview dialog gets destroyed
  // and reaches RemovePreviewDialog(), it does not attempt to also remove the
  // initiator's observers.
  EraseInitiatorInfo(preview_dialog);

  PrintViewManager::FromWebContents(initiator)->PrintPreviewDone();

  // initiator is closed. Close the print preview dialog too.
  if (content::WebUI* web_ui = preview_dialog->GetWebUI()) {
    PrintPreviewUI* print_preview_ui =
        static_cast<PrintPreviewUI*>(web_ui->GetController());
    if (print_preview_ui)
      print_preview_ui->OnInitiatorClosed();
  }
}

void PrintPreviewDialogController::RemovePreviewDialog(
    WebContents* preview_dialog) {
  for (size_t i = 0; i < preview_operations_.size(); ++i) {
    Operation* operation = preview_operations_[i];
    if (operation->preview_dialog == preview_dialog) {
      // Remove the initiator's observers before erasing the mapping.
      if (operation->initiator) {
        operation->initiator_observer.StopObserving();
        PrintViewManager::FromWebContents(operation->initiator)->
            PrintPreviewDone();
      }

      // Print preview WebContents is destroyed. Notify |PrintPreviewUI| to
      // abort the initiator preview request.
      if (content::WebUI* web_ui = preview_dialog->GetWebUI()) {
        PrintPreviewUI* print_preview_ui =
            static_cast<PrintPreviewUI*>(web_ui->GetController());
        if (print_preview_ui)
          print_preview_ui->OnPrintPreviewDialogDestroyed();
      }

      preview_operations_.erase(preview_operations_.begin() + i);
      delete operation;

      return;
    }
  }
  NOTREACHED();
}

}  // namespace printing
