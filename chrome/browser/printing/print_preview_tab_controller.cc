// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_tab_controller.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/web_dialogs/constrained_web_dialog_ui.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"
#include "webkit/plugins/webplugininfo.h"

using content::NativeWebKeyboardEvent;
using content::NavigationController;
using content::WebContents;
using content::WebUIMessageHandler;
using ui::ConstrainedWebDialogDelegate;
using ui::WebDialogDelegate;
using ui::WebDialogWebContentsDelegate;

namespace {

void EnableInternalPDFPluginForTab(TabContents* preview_tab) {
  // Always enable the internal PDF plugin for the print preview page.
  ChromePluginServiceFilter::GetInstance()->OverridePluginForTab(
        preview_tab->web_contents()->GetRenderProcessHost()->GetID(),
        preview_tab->web_contents()->GetRenderViewHost()->GetRoutingID(),
        GURL(),
        ASCIIToUTF16(chrome::ChromeContentClient::kPDFPluginName));
}

// WebDialogDelegate that specifies what the print preview dialog
// will look like.
class PrintPreviewTabDelegate : public WebDialogDelegate {
 public:
  explicit PrintPreviewTabDelegate(TabContents* initiator_tab);
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
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewTabDelegate);
};

PrintPreviewTabDelegate::PrintPreviewTabDelegate(TabContents* initiator_tab) {
  const gfx::Size kMinDialogSize(800, 480);
  const int kBorder = 50;
  gfx::Rect rect;
  initiator_tab->web_contents()->GetContainerBounds(&rect);
  size_.set_width(std::max(rect.width(), kMinDialogSize.width()) - kBorder);
  size_.set_height(std::max(rect.height(), kMinDialogSize.height()) - kBorder);

#if defined(OS_MACOSX)
  // Limit the maximum size on MacOS X.
  // http://crbug.com/105815
  const gfx::Size kMaxDialogSize(1000, 660);
  size_.set_width(std::min(size_.width(), kMaxDialogSize.width()));
  size_.set_height(std::min(size_.height(), kMaxDialogSize.height()));
#endif
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
  *size = size_;
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
// renderer to the browser and makes sure users can't reload/save.
class PrintPreviewWebContentDelegate : public WebDialogWebContentsDelegate,
                                       public CoreTabHelperDelegate {
 public:
  PrintPreviewWebContentDelegate(Profile* profile, TabContents* initiator_tab);
  virtual ~PrintPreviewWebContentDelegate();

  // Overridden from WebDialogWebContentsDelegate:
  virtual void HandleKeyboardEvent(
      WebContents* source,
      const NativeWebKeyboardEvent& event) OVERRIDE;

  // Overridden from CoreTabHelperDelegate:
  virtual bool CanReloadContents(TabContents* source) const OVERRIDE;
  virtual bool CanSaveContents(TabContents* source) const OVERRIDE;

 private:
  TabContents* tab_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewWebContentDelegate);
};

PrintPreviewWebContentDelegate::PrintPreviewWebContentDelegate(
    Profile* profile,
    TabContents* initiator_tab)
    : WebDialogWebContentsDelegate(profile, new ChromeWebContentsHandler),
      tab_(initiator_tab) {}

PrintPreviewWebContentDelegate::~PrintPreviewWebContentDelegate() {}

bool PrintPreviewWebContentDelegate::CanReloadContents(
    TabContents* source) const {
  return false;
}

bool PrintPreviewWebContentDelegate::CanSaveContents(
    TabContents* source) const {
  return false;
}

void PrintPreviewWebContentDelegate::HandleKeyboardEvent(
    WebContents* source,
    const NativeWebKeyboardEvent& event) {
  // Disabled on Mac due to http://crbug.com/112173
#if !defined(OS_MACOSX)
  Browser* current_browser =
      browser::FindBrowserWithWebContents(tab_->web_contents());
  if (!current_browser)
    return;
  current_browser->window()->HandleKeyboardEvent(event);
#endif
}

}  // namespace

namespace printing {

PrintPreviewTabController::PrintPreviewTabController()
    : waiting_for_new_preview_page_(false),
      is_creating_print_preview_tab_(false) {
}

// static
PrintPreviewTabController* PrintPreviewTabController::GetInstance() {
  if (!g_browser_process)
    return NULL;
  return g_browser_process->print_preview_tab_controller();
}

// static
void PrintPreviewTabController::PrintPreview(TabContents* tab) {
  if (tab->web_contents()->ShowingInterstitialPage())
    return;

  PrintPreviewTabController* tab_controller = GetInstance();
  if (!tab_controller)
    return;
  if (!tab_controller->GetOrCreatePreviewTab(tab))
    tab->print_view_manager()->PrintPreviewDone();
}

TabContents* PrintPreviewTabController::GetOrCreatePreviewTab(
    TabContents* initiator_tab) {
  DCHECK(initiator_tab);

  // Get the print preview tab for |initiator_tab|.
  TabContents* preview_tab = GetPrintPreviewForTab(initiator_tab);
  if (!preview_tab)
    return CreatePrintPreviewTab(initiator_tab);

  // Show the initiator tab holding the existing preview tab.
  WebContents* web_contents = initiator_tab->web_contents();
  web_contents->GetDelegate()->ActivateContents(web_contents);
  return preview_tab;
}

TabContents* PrintPreviewTabController::GetPrintPreviewForTab(
    TabContents* tab) const {
  // |preview_tab_map_| is keyed by the preview tab, so if find() succeeds, then
  // |tab| is the preview tab.
  PrintPreviewTabMap::const_iterator it = preview_tab_map_.find(tab);
  if (it != preview_tab_map_.end())
    return tab;

  for (it = preview_tab_map_.begin(); it != preview_tab_map_.end(); ++it) {
    // If |tab| is an initiator tab.
    if (tab == it->second) {
      // Return the associated preview tab.
      return it->first;
    }
  }
  return NULL;
}

TabContents* PrintPreviewTabController::GetInitiatorTab(
    TabContents* preview_tab) {
  PrintPreviewTabMap::iterator it = preview_tab_map_.find(preview_tab);
  if (it != preview_tab_map_.end())
    return preview_tab_map_[preview_tab];
  return NULL;
}

void PrintPreviewTabController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_RENDERER_PROCESS_CLOSED) {
    OnRendererProcessClosed(
        content::Source<content::RenderProcessHost>(source).ptr());
  } else if (type == chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED) {
    OnTabContentsDestroyed(content::Source<TabContents>(source).ptr());
  } else {
    DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);
    TabContents* tab = TabContents::FromWebContents(
        content::Source<NavigationController>(source)->GetWebContents());
    OnNavEntryCommitted(tab,
        content::Details<content::LoadCommittedDetails>(details).ptr());
  }
}

// static
bool PrintPreviewTabController::IsPrintPreviewTab(TabContents* tab) {
  return IsPrintPreviewURL(tab->web_contents()->GetURL());
}

// static
bool PrintPreviewTabController::IsPrintPreviewURL(const GURL& url) {
  return (url.SchemeIs(chrome::kChromeUIScheme) &&
          url.host() == chrome::kChromeUIPrintHost);
}

void PrintPreviewTabController::EraseInitiatorTabInfo(
    TabContents* preview_tab) {
  PrintPreviewTabMap::iterator it = preview_tab_map_.find(preview_tab);
  if (it == preview_tab_map_.end())
    return;

  RemoveObservers(it->second);
  preview_tab_map_[preview_tab] = NULL;
}

bool PrintPreviewTabController::is_creating_print_preview_tab() const {
  return is_creating_print_preview_tab_;
}

PrintPreviewTabController::~PrintPreviewTabController() {}

void PrintPreviewTabController::OnRendererProcessClosed(
    content::RenderProcessHost* rph) {
  // Store tabs in a vector and deal with them after iterating through
  // |preview_tab_map_| because RemoveFooTab() can change |preview_tab_map_|.
  std::vector<TabContents*> closed_initiator_tabs;
  std::vector<TabContents*> closed_preview_tabs;
  for (PrintPreviewTabMap::iterator iter = preview_tab_map_.begin();
       iter != preview_tab_map_.end(); ++iter) {
    TabContents* preview_tab = iter->first;
    TabContents* initiator_tab = iter->second;
    if (preview_tab->web_contents()->GetRenderProcessHost() == rph) {
      closed_preview_tabs.push_back(preview_tab);
    } else if (initiator_tab &&
               initiator_tab->web_contents()->GetRenderProcessHost() == rph) {
      closed_initiator_tabs.push_back(initiator_tab);
    }
  }

  for (size_t i = 0; i < closed_preview_tabs.size(); ++i) {
    RemovePreviewTab(closed_preview_tabs[i]);
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
        closed_preview_tabs[i]->web_contents()->GetWebUI()->GetController());
    if (print_preview_ui)
      print_preview_ui->OnPrintPreviewTabClosed();
  }

  for (size_t i = 0; i < closed_initiator_tabs.size(); ++i)
    RemoveInitiatorTab(closed_initiator_tabs[i]);
}

void PrintPreviewTabController::OnTabContentsDestroyed(TabContents* tab) {
  TabContents* preview_tab = GetPrintPreviewForTab(tab);
  if (!preview_tab) {
    NOTREACHED();
    return;
  }

  if (tab == preview_tab)
    RemovePreviewTab(tab);
  else
    RemoveInitiatorTab(tab);
}

void PrintPreviewTabController::OnNavEntryCommitted(
    TabContents* tab, content::LoadCommittedDetails* details) {
  TabContents* preview_tab = GetPrintPreviewForTab(tab);
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
        SetInitiatorTabURLAndTitle(preview_tab);
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

TabContents* PrintPreviewTabController::CreatePrintPreviewTab(
    TabContents* initiator_tab) {
  AutoReset<bool> auto_reset(&is_creating_print_preview_tab_, true);
  WebContents* web_contents = initiator_tab->web_contents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame)) {
    Browser* current_browser = new Browser(
        Browser::CreateParams(Browser::TYPE_POPUP, profile));
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
      ui::CreateConstrainedWebDialog(
          profile,
          web_dialog_delegate,
          pp_wcd,
          initiator_tab);
  TabContents* preview_tab = constrained_delegate->tab();
  EnableInternalPDFPluginForTab(preview_tab);
  preview_tab->core_tab_helper()->set_delegate(pp_wcd);

  // Add an entry to the map.
  preview_tab_map_[preview_tab] = initiator_tab;
  waiting_for_new_preview_page_ = true;

  AddObservers(initiator_tab);
  AddObservers(preview_tab);

  return preview_tab;
}

void PrintPreviewTabController::SetInitiatorTabURLAndTitle(
    TabContents* preview_tab) {
  TabContents* initiator_tab = GetInitiatorTab(preview_tab);
  if (initiator_tab && preview_tab->web_contents()->GetWebUI()) {
    PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
        preview_tab->web_contents()->GetWebUI()->GetController());
    print_preview_ui->SetInitiatorTabURLAndTitle(
        initiator_tab->web_contents()->GetURL().spec(),
        initiator_tab->print_view_manager()->RenderSourceName());
  }
}

void PrintPreviewTabController::AddObservers(TabContents* tab) {
  WebContents* web_contents = tab->web_contents();
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                 content::Source<TabContents>(tab));
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&web_contents->GetController()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  content::Source<content::RenderProcessHost> rph_source(
      web_contents->GetRenderProcessHost());
  if (!registrar_.IsRegistered(this,
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED, rph_source)) {
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   rph_source);
  }
}

void PrintPreviewTabController::RemoveObservers(TabContents* tab) {
  WebContents* web_contents = tab->web_contents();
  registrar_.Remove(this, chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                    content::Source<TabContents>(tab));
  registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&web_contents->GetController()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  content::Source<content::RenderProcessHost> rph_source(
      web_contents->GetRenderProcessHost());
  if (registrar_.IsRegistered(this,
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED, rph_source)) {
    registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                      rph_source);
  }
}

void PrintPreviewTabController::RemoveInitiatorTab(TabContents* initiator_tab) {
  TabContents* preview_tab = GetPrintPreviewForTab(initiator_tab);
  DCHECK(preview_tab);
  // Update the map entry first, so when the print preview tab gets destroyed
  // and reaches RemovePreviewTab(), it does not attempt to also remove the
  // initiator tab's observers.
  preview_tab_map_[preview_tab] = NULL;
  RemoveObservers(initiator_tab);

  initiator_tab->print_view_manager()->PrintPreviewDone();

  // Initiator tab is closed. Close the print preview tab too.
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      preview_tab->web_contents()->GetWebUI()->GetController());
  if (print_preview_ui)
    print_preview_ui->OnInitiatorTabClosed();
}

void PrintPreviewTabController::RemovePreviewTab(TabContents* preview_tab) {
  // Remove the initiator tab's observers before erasing the mapping.
  TabContents* initiator_tab = GetInitiatorTab(preview_tab);
  if (initiator_tab) {
    RemoveObservers(initiator_tab);
    initiator_tab->print_view_manager()->PrintPreviewDone();
  }

  // Print preview WebContents is destroyed. Notify |PrintPreviewUI| to abort
  // the initiator tab preview request.
  PrintPreviewUI* print_preview_ui = static_cast<PrintPreviewUI*>(
      preview_tab->web_contents()->GetWebUI()->GetController());
  if (print_preview_ui)
    print_preview_ui->OnTabDestroyed();

  preview_tab_map_.erase(preview_tab);
  RemoveObservers(preview_tab);
}

}  // namespace printing
