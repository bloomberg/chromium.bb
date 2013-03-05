// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_dialog_controller.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"
#include "webkit/plugins/webplugininfo.h"

using content::NativeWebKeyboardEvent;
using content::NavigationController;
using content::WebContents;
using content::WebUIMessageHandler;
using ui::WebDialogDelegate;
using ui::WebDialogWebContentsDelegate;

namespace {

void EnableInternalPDFPluginForTab(WebContents* preview_tab) {
  // Always enable the internal PDF plugin for the print preview page.
  base::FilePath pdf_plugin_path;
  if (!PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_plugin_path))
    return;

  webkit::WebPluginInfo pdf_plugin;
  if (!content::PluginService::GetInstance()->GetPluginInfoByPath(
      pdf_plugin_path, &pdf_plugin))
    return;

  ChromePluginServiceFilter::GetInstance()->OverridePluginForTab(
      preview_tab->GetRenderProcessHost()->GetID(),
      preview_tab->GetRenderViewHost()->GetRoutingID(),
      GURL(), pdf_plugin);
}

// WebDialogDelegate that specifies what the print preview dialog
// will look like.
class PrintPreviewTabDelegate : public WebDialogDelegate {
 public:
  explicit PrintPreviewTabDelegate(WebContents* initiator_tab);
  virtual ~PrintPreviewTabDelegate();

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

 private:
  WebContents* initiator_tab_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewTabDelegate);
};

PrintPreviewTabDelegate::PrintPreviewTabDelegate(WebContents* initiator_tab) {
  initiator_tab_ = initiator_tab;
}

PrintPreviewTabDelegate::~PrintPreviewTabDelegate() {
}

ui::ModalType PrintPreviewTabDelegate::GetDialogModalType() const {
  // Not used, returning dummy value.
  NOTREACHED();
  return ui::MODAL_TYPE_WINDOW;
}

string16 PrintPreviewTabDelegate::GetDialogTitle() const {
  // Only used on Windows? UI folks prefer no title.
  return string16();
}

GURL PrintPreviewTabDelegate::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIPrintURL);
}

void PrintPreviewTabDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* /* handlers */) const {
  // PrintPreviewUI adds its own message handlers.
}

void PrintPreviewTabDelegate::GetDialogSize(gfx::Size* size) const {
  DCHECK(size);
  const gfx::Size kMinDialogSize(800, 480);
  const int kBorder = 50;
  gfx::Rect rect;
  initiator_tab_->GetView()->GetContainerBounds(&rect);
  size->set_width(std::max(rect.width(), kMinDialogSize.width()) - kBorder);
  size->set_height(std::max(rect.height(), kMinDialogSize.height()) - kBorder);

#if defined(OS_MACOSX)
  // Limit the maximum size on MacOS X.
  // http://crbug.com/105815
  const gfx::Size kMaxDialogSize(1000, 660);
  size->set_width(std::min(size->width(), kMaxDialogSize.width()));
  size->set_height(std::min(size->height(), kMaxDialogSize.height()));
#endif
}

std::string PrintPreviewTabDelegate::GetDialogArgs() const {
  return std::string();
}

void PrintPreviewTabDelegate::OnDialogClosed(
    const std::string& /* json_retval */) {
}

void PrintPreviewTabDelegate::OnCloseContents(WebContents* /* source */,
                                              bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool PrintPreviewTabDelegate::ShouldShowDialogTitle() const {
  // Not used, returning dummy value.
  NOTREACHED();
  return false;
}

// WebContentsDelegate that forwards shortcut keys in the print preview
// renderer to the browser.
class PrintPreviewWebContentDelegate : public WebDialogWebContentsDelegate {
 public:
  PrintPreviewWebContentDelegate(Profile* profile, WebContents* initiator_tab);
  virtual ~PrintPreviewWebContentDelegate();

  // Overridden from WebDialogWebContentsDelegate:
  virtual void HandleKeyboardEvent(
      WebContents* source,
      const NativeWebKeyboardEvent& event) OVERRIDE;

 private:
  WebContents* tab_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewWebContentDelegate);
};

PrintPreviewWebContentDelegate::PrintPreviewWebContentDelegate(
    Profile* profile,
    WebContents* initiator_tab)
    : WebDialogWebContentsDelegate(profile, new ChromeWebContentsHandler),
      tab_(initiator_tab) {}

PrintPreviewWebContentDelegate::~PrintPreviewWebContentDelegate() {}

void PrintPreviewWebContentDelegate::HandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event) {
  // Disabled on Mac due to http://crbug.com/112173
#if !defined(OS_MACOSX)
  Browser* current_browser = chrome::FindBrowserWithWebContents(tab_);
  if (!current_browser)
    return;
  current_browser->window()->HandleKeyboardEvent(event);
#endif
}

}  // namespace

namespace printing {

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
void PrintPreviewDialogController::PrintPreview(WebContents* initiator_tab) {
  if (initiator_tab->ShowingInterstitialPage())
    return;

  PrintPreviewDialogController* dialog_controller = GetInstance();
  if (!dialog_controller)
    return;
  if (!dialog_controller->GetOrCreatePreviewDialog(initiator_tab))
    PrintViewManager::FromWebContents(initiator_tab)->PrintPreviewDone();
}

WebContents* PrintPreviewDialogController::GetOrCreatePreviewDialog(
    WebContents* initiator_tab) {
  DCHECK(initiator_tab);

  // Get the print preview dialog for |initiator_tab|.
  WebContents* preview_dialog = GetPrintPreviewForContents(initiator_tab);
  if (!preview_dialog)
    return CreatePrintPreviewTab(initiator_tab);

  // Show the initiator tab holding the existing preview dialog.
  initiator_tab->GetDelegate()->ActivateContents(initiator_tab);
  return preview_dialog;
}

WebContents* PrintPreviewDialogController::GetPrintPreviewForContents(
    WebContents* contents) const {
  // |preview_tab_map_| is keyed by the preview dialog, so if find() succeeds,
  // then |contents| is the preview dialog.
  PrintPreviewTabMap::const_iterator it = preview_tab_map_.find(contents);
  if (it != preview_tab_map_.end())
    return contents;

  for (it = preview_tab_map_.begin(); it != preview_tab_map_.end(); ++it) {
    // If |contents| is an initiator tab.
    if (contents == it->second) {
      // Return the associated preview tab.
      return it->first;
    }
  }
  return NULL;
}

WebContents* PrintPreviewDialogController::GetInitiatorTab(
    WebContents* preview_tab) {
  PrintPreviewTabMap::iterator it = preview_tab_map_.find(preview_tab);
  if (it != preview_tab_map_.end())
    return preview_tab_map_[preview_tab];
  return NULL;
}

void PrintPreviewDialogController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_RENDERER_PROCESS_CLOSED) {
    OnRendererProcessClosed(
        content::Source<content::RenderProcessHost>(source).ptr());
  } else if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    OnWebContentsDestroyed(content::Source<WebContents>(source).ptr());
  } else {
    DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
    WebContents* tab =
        content::Source<NavigationController>(source)->GetWebContents();
    OnNavEntryCommitted(tab,
        content::Details<content::LoadCommittedDetails>(details).ptr());
  }
}

// static
bool PrintPreviewDialogController::IsPrintPreviewDialog(WebContents* contents) {
  return IsPrintPreviewURL(contents->GetURL());
}

// static
bool PrintPreviewDialogController::IsPrintPreviewURL(const GURL& url) {
  return (url.SchemeIs(chrome::kChromeUIScheme) &&
          url.host() == chrome::kChromeUIPrintHost);
}

void PrintPreviewDialogController::EraseInitiatorTabInfo(
    WebContents* preview_tab) {
  PrintPreviewTabMap::iterator it = preview_tab_map_.find(preview_tab);
  if (it == preview_tab_map_.end())
    return;

  RemoveObservers(it->second);
  preview_tab_map_[preview_tab] = NULL;
}

PrintPreviewDialogController::~PrintPreviewDialogController() {}

void PrintPreviewDialogController::OnRendererProcessClosed(
    content::RenderProcessHost* rph) {
  // Store tabs in a vector and deal with them after iterating through
  // |preview_tab_map_| because RemoveFooTab() can change |preview_tab_map_|.
  std::vector<WebContents*> closed_initiator_tabs;
  std::vector<WebContents*> closed_preview_tabs;
  for (PrintPreviewTabMap::iterator iter = preview_tab_map_.begin();
       iter != preview_tab_map_.end(); ++iter) {
    WebContents* preview_tab = iter->first;
    WebContents* initiator_tab = iter->second;
    if (preview_tab->GetRenderProcessHost() == rph) {
      closed_preview_tabs.push_back(preview_tab);
    } else if (initiator_tab &&
               initiator_tab->GetRenderProcessHost() == rph) {
      closed_initiator_tabs.push_back(initiator_tab);
    }
  }

  for (size_t i = 0; i < closed_preview_tabs.size(); ++i) {
    RemovePreviewTab(closed_preview_tabs[i]);
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
        closed_preview_tabs[i]->GetWebUI()->GetController());
    if (print_preview_ui)
      print_preview_ui->OnPrintPreviewDialogClosed();
  }

  for (size_t i = 0; i < closed_initiator_tabs.size(); ++i)
    RemoveInitiatorTab(closed_initiator_tabs[i]);
}

void PrintPreviewDialogController::OnWebContentsDestroyed(WebContents* tab) {
  WebContents* preview_tab = GetPrintPreviewForContents(tab);
  if (!preview_tab) {
    NOTREACHED();
    return;
  }

  if (tab == preview_tab)
    RemovePreviewTab(tab);
  else
    RemoveInitiatorTab(tab);
}

void PrintPreviewDialogController::OnNavEntryCommitted(
    WebContents* tab, content::LoadCommittedDetails* details) {
  WebContents* preview_tab = GetPrintPreviewForContents(tab);
  if (!preview_tab) {
    NOTREACHED();
    return;
  }
  bool source_tab_is_preview_tab = (tab == preview_tab);

  if (source_tab_is_preview_tab) {
    // Preview tab navigated.
    if (details) {
      content::PageTransition transition_type =
          details->entry->GetTransitionType();
      content::NavigationType nav_type = details->type;

      // New |preview_tab| is created. Don't update/erase map entry.
      if (waiting_for_new_preview_page_ &&
          transition_type == content::PAGE_TRANSITION_AUTO_TOPLEVEL &&
          nav_type == content::NAVIGATION_TYPE_NEW_PAGE) {
        waiting_for_new_preview_page_ = false;
        SaveInitiatorTabTitle(preview_tab);
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

  RemoveInitiatorTab(tab);
}

WebContents* PrintPreviewDialogController::CreatePrintPreviewTab(
    WebContents* initiator_tab) {
  base::AutoReset<bool> auto_reset(&is_creating_print_preview_dialog_, true);
  Profile* profile =
      Profile::FromBrowserContext(initiator_tab->GetBrowserContext());
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame)) {
    // Chrome Frame only ever runs on the native desktop, so it is safe to
    // create the popup on the native desktop.
    Browser* current_browser = new Browser(
        Browser::CreateParams(Browser::TYPE_POPUP, profile,
                              chrome::HOST_DESKTOP_TYPE_NATIVE));
    if (!current_browser) {
      NOTREACHED() << "Failed to create popup browser window";
      return NULL;
    }
  }

  // |web_dialog_ui_delegate| deletes itself in
  // PrintPreviewTabDelegate::OnDialogClosed().
  WebDialogDelegate* web_dialog_delegate =
      new PrintPreviewTabDelegate(initiator_tab);
  // |web_tab_content_delegate|'s owner is |constrained_web_ui_delegate|.
  PrintPreviewWebContentDelegate* pp_wcd =
      new PrintPreviewWebContentDelegate(profile, initiator_tab);
  ConstrainedWebDialogDelegate* constrained_delegate =
      CreateConstrainedWebDialog(profile,
                                 web_dialog_delegate,
                                 pp_wcd,
                                 initiator_tab);
  WebContents* preview_tab = constrained_delegate->GetWebContents();
  EnableInternalPDFPluginForTab(preview_tab);
  PrintViewManager::CreateForWebContents(preview_tab);

  // Add an entry to the map.
  preview_tab_map_[preview_tab] = initiator_tab;
  waiting_for_new_preview_page_ = true;

  AddObservers(initiator_tab);
  AddObservers(preview_tab);

  return preview_tab;
}

void PrintPreviewDialogController::SaveInitiatorTabTitle(
    WebContents* preview_dialog) {
  WebContents* initiator_tab = GetInitiatorTab(preview_dialog);
  if (initiator_tab && preview_dialog->GetWebUI()) {
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
        preview_dialog->GetWebUI()->GetController());
    print_preview_ui->SetInitiatorTabTitle(
        PrintViewManager::FromWebContents(initiator_tab)->RenderSourceName());
  }
}

void PrintPreviewDialogController::AddObservers(WebContents* tab) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(tab));
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&tab->GetController()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  content::Source<content::RenderProcessHost> rph_source(
      tab->GetRenderProcessHost());
  if (!registrar_.IsRegistered(this,
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED, rph_source)) {
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   rph_source);
  }
}

void PrintPreviewDialogController::RemoveObservers(WebContents* tab) {
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    content::Source<WebContents>(tab));
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&tab->GetController()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  content::Source<content::RenderProcessHost> rph_source(
      tab->GetRenderProcessHost());
  if (registrar_.IsRegistered(this,
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED, rph_source)) {
    registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                      rph_source);
  }
}

void PrintPreviewDialogController::RemoveInitiatorTab(
    WebContents* initiator_tab) {
  WebContents* preview_tab = GetPrintPreviewForContents(initiator_tab);
  DCHECK(preview_tab);
  // Update the map entry first, so when the print preview tab gets destroyed
  // and reaches RemovePreviewTab(), it does not attempt to also remove the
  // initiator tab's observers.
  preview_tab_map_[preview_tab] = NULL;
  RemoveObservers(initiator_tab);

  PrintViewManager::FromWebContents(initiator_tab)->PrintPreviewDone();

  // Initiator tab is closed. Close the print preview tab too.
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      preview_tab->GetWebUI()->GetController());
  if (print_preview_ui)
    print_preview_ui->OnInitiatorTabClosed();
}

void PrintPreviewDialogController::RemovePreviewTab(WebContents* preview_tab) {
  // Remove the initiator tab's observers before erasing the mapping.
  WebContents* initiator_tab = GetInitiatorTab(preview_tab);
  if (initiator_tab) {
    RemoveObservers(initiator_tab);
    PrintViewManager::FromWebContents(initiator_tab)->PrintPreviewDone();
  }

  // Print preview WebContents is destroyed. Notify |PrintPreviewUI| to abort
  // the initiator tab preview request.
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      preview_tab->GetWebUI()->GetController());
  if (print_preview_ui)
    print_preview_ui->OnPrintPreviewDialogDestroyed();

  preview_tab_map_.erase(preview_tab);
  RemoveObservers(preview_tab);
}

}  // namespace printing
