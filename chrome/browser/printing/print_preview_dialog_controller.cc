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

void EnableInternalPDFPluginForContents(WebContents* preview_dialog) {
  // Always enable the internal PDF plugin for the print preview page.
  base::FilePath pdf_plugin_path;
  if (!PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_plugin_path))
    return;

  webkit::WebPluginInfo pdf_plugin;
  if (!content::PluginService::GetInstance()->GetPluginInfoByPath(
      pdf_plugin_path, &pdf_plugin))
    return;

  ChromePluginServiceFilter::GetInstance()->OverridePluginForTab(
      preview_dialog->GetRenderProcessHost()->GetID(),
      preview_dialog->GetRenderViewHost()->GetRoutingID(),
      GURL(), pdf_plugin);
}

// WebDialogDelegate that specifies what the print preview dialog
// will look like.
class PrintPreviewDialogDelegate : public WebDialogDelegate {
 public:
  explicit PrintPreviewDialogDelegate(WebContents* initiator_tab);
  virtual ~PrintPreviewDialogDelegate();

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

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDialogDelegate);
};

PrintPreviewDialogDelegate::PrintPreviewDialogDelegate(
    WebContents* initiator_tab) {
  initiator_tab_ = initiator_tab;
}

PrintPreviewDialogDelegate::~PrintPreviewDialogDelegate() {
}

ui::ModalType PrintPreviewDialogDelegate::GetDialogModalType() const {
  // Not used, returning dummy value.
  NOTREACHED();
  return ui::MODAL_TYPE_WINDOW;
}

string16 PrintPreviewDialogDelegate::GetDialogTitle() const {
  // Only used on Windows? UI folks prefer no title.
  return string16();
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
    return CreatePrintPreviewDialog(initiator_tab);

  // Show the initiator tab holding the existing preview dialog.
  initiator_tab->GetDelegate()->ActivateContents(initiator_tab);
  return preview_dialog;
}

WebContents* PrintPreviewDialogController::GetPrintPreviewForContents(
    WebContents* contents) const {
  // |preview_dialog_map_| is keyed by the preview dialog, so if find()
  // succeeds, then |contents| is the preview dialog.
  PrintPreviewDialogMap::const_iterator it = preview_dialog_map_.find(contents);
  if (it != preview_dialog_map_.end())
    return contents;

  for (it = preview_dialog_map_.begin();
       it != preview_dialog_map_.end();
       ++it) {
    // If |contents| is an initiator tab.
    if (contents == it->second) {
      // Return the associated preview dialog.
      return it->first;
    }
  }
  return NULL;
}

WebContents* PrintPreviewDialogController::GetInitiatorTab(
    WebContents* preview_dialog) {
  PrintPreviewDialogMap::iterator it = preview_dialog_map_.find(preview_dialog);
  return (it != preview_dialog_map_.end()) ? it->second : NULL;
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
    WebContents* contents =
        content::Source<NavigationController>(source)->GetWebContents();
    OnNavEntryCommitted(
        contents,
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
    WebContents* preview_dialog) {
  PrintPreviewDialogMap::iterator it = preview_dialog_map_.find(preview_dialog);
  if (it == preview_dialog_map_.end())
    return;

  RemoveObservers(it->second);
  preview_dialog_map_[preview_dialog] = NULL;
}

PrintPreviewDialogController::~PrintPreviewDialogController() {}

void PrintPreviewDialogController::OnRendererProcessClosed(
    content::RenderProcessHost* rph) {
  // Store contents in a vector and deal with them after iterating through
  // |preview_dialog_map_| because RemoveFoo() can change |preview_dialog_map_|.
  std::vector<WebContents*> closed_initiator_tabs;
  std::vector<WebContents*> closed_preview_dialogs;
  for (PrintPreviewDialogMap::iterator iter = preview_dialog_map_.begin();
       iter != preview_dialog_map_.end(); ++iter) {
    WebContents* preview_dialog = iter->first;
    WebContents* initiator_tab = iter->second;
    if (preview_dialog->GetRenderProcessHost() == rph) {
      closed_preview_dialogs.push_back(preview_dialog);
    } else if (initiator_tab &&
               initiator_tab->GetRenderProcessHost() == rph) {
      closed_initiator_tabs.push_back(initiator_tab);
    }
  }

  for (size_t i = 0; i < closed_preview_dialogs.size(); ++i) {
    RemovePreviewDialog(closed_preview_dialogs[i]);
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
        closed_preview_dialogs[i]->GetWebUI()->GetController());
    if (print_preview_ui)
      print_preview_ui->OnPrintPreviewDialogClosed();
  }

  for (size_t i = 0; i < closed_initiator_tabs.size(); ++i)
    RemoveInitiatorTab(closed_initiator_tabs[i]);
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
    RemoveInitiatorTab(contents);
}

void PrintPreviewDialogController::OnNavEntryCommitted(
    WebContents* contents, content::LoadCommittedDetails* details) {
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
        SaveInitiatorTabTitle(preview_dialog);
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

  RemoveInitiatorTab(contents);
}

WebContents* PrintPreviewDialogController::CreatePrintPreviewDialog(
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
  // PrintPreviewDialogDelegate::OnDialogClosed().
  WebDialogDelegate* web_dialog_delegate =
      new PrintPreviewDialogDelegate(initiator_tab);
  // |web_dialog_delegate|'s owner is |constrained_delegate|.
  PrintPreviewWebContentDelegate* pp_wcd =
      new PrintPreviewWebContentDelegate(profile, initiator_tab);
  ConstrainedWebDialogDelegate* constrained_delegate =
      CreateConstrainedWebDialog(profile,
                                 web_dialog_delegate,
                                 pp_wcd,
                                 initiator_tab);
  WebContents* preview_dialog = constrained_delegate->GetWebContents();
  EnableInternalPDFPluginForContents(preview_dialog);
  PrintViewManager::CreateForWebContents(preview_dialog);

  // Add an entry to the map.
  preview_dialog_map_[preview_dialog] = initiator_tab;
  waiting_for_new_preview_page_ = true;

  AddObservers(initiator_tab);
  AddObservers(preview_dialog);

  return preview_dialog;
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

void PrintPreviewDialogController::AddObservers(WebContents* contents) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(contents));
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&contents->GetController()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  content::Source<content::RenderProcessHost> rph_source(
      contents->GetRenderProcessHost());
  if (!registrar_.IsRegistered(this,
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED, rph_source)) {
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   rph_source);
  }
}

void PrintPreviewDialogController::RemoveObservers(WebContents* contents) {
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    content::Source<WebContents>(contents));
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&contents->GetController()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  content::Source<content::RenderProcessHost> rph_source(
      contents->GetRenderProcessHost());
  if (registrar_.IsRegistered(this,
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED, rph_source)) {
    registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                      rph_source);
  }
}

void PrintPreviewDialogController::RemoveInitiatorTab(
    WebContents* initiator_tab) {
  WebContents* preview_dialog = GetPrintPreviewForContents(initiator_tab);
  DCHECK(preview_dialog);
  // Update the map entry first, so when the print preview dialog gets destroyed
  // and reaches RemovePreviewDialog(), it does not attempt to also remove the
  // initiator tab's observers.
  preview_dialog_map_[preview_dialog] = NULL;
  RemoveObservers(initiator_tab);

  PrintViewManager::FromWebContents(initiator_tab)->PrintPreviewDone();

  // Initiator tab is closed. Close the print preview dialog too.
  PrintPreviewUI* print_preview_ui =
      static_cast<PrintPreviewUI*>(preview_dialog->GetWebUI()->GetController());
  if (print_preview_ui)
    print_preview_ui->OnInitiatorTabClosed();
}

void PrintPreviewDialogController::RemovePreviewDialog(
    WebContents* preview_dialog) {
  // Remove the initiator tab's observers before erasing the mapping.
  WebContents* initiator_tab = GetInitiatorTab(preview_dialog);
  if (initiator_tab) {
    RemoveObservers(initiator_tab);
    PrintViewManager::FromWebContents(initiator_tab)->PrintPreviewDone();
  }

  // Print preview WebContents is destroyed. Notify |PrintPreviewUI| to abort
  // the initiator tab preview request.
  PrintPreviewUI* print_preview_ui =
      static_cast<PrintPreviewUI*>(preview_dialog->GetWebUI()->GetController());
  if (print_preview_ui)
    print_preview_ui->OnPrintPreviewDialogDestroyed();

  preview_dialog_map_.erase(preview_dialog);
  RemoveObservers(preview_dialog);
}

}  // namespace printing
