// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_tab_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/constrained_html_ui.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "webkit/plugins/webplugininfo.h"

namespace {

void EnableInternalPDFPluginForTab(TabContentsWrapper* preview_tab) {
  // Always enable the internal PDF plugin for the print preview page.
  ChromePluginServiceFilter::GetInstance()->OverridePluginForTab(
        preview_tab->render_view_host()->process()->GetID(),
        preview_tab->render_view_host()->routing_id(),
        GURL(),
        ASCIIToUTF16(chrome::ChromeContentClient::kPDFPluginName));
}

class PrintPreviewTabDelegate : public HtmlDialogUIDelegate {
 public:
  explicit PrintPreviewTabDelegate(TabContentsWrapper* initiator_tab);
  virtual ~PrintPreviewTabDelegate();

  virtual bool IsDialogModal() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(TabContents* source,
                               bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewTabDelegate);
};

PrintPreviewTabDelegate::PrintPreviewTabDelegate(
    TabContentsWrapper* initiator_tab) {
  const gfx::Size kMinDialogSize(800, 480);
  const int kBorder = 50;
  gfx::Rect rect;
  initiator_tab->tab_contents()->GetContainerBounds(&rect);
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

bool PrintPreviewTabDelegate::IsDialogModal() const {
  // Not used, returning dummy value.
  NOTREACHED();
  return true;
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
  delete this;
}

void PrintPreviewTabDelegate::OnCloseContents(TabContents* /* source */,
                                              bool* /* out_close_dialog */) {
  // Not used, returning dummy value.
  NOTREACHED();
}

bool PrintPreviewTabDelegate::ShouldShowDialogTitle() const {
  // Not used, returning dummy value.
  NOTREACHED();
  return false;
}

}  // namespace

namespace printing {

PrintPreviewTabController::PrintPreviewTabController()
    : waiting_for_new_preview_page_(false),
      is_creating_print_preview_tab_(false) {
}

PrintPreviewTabController::~PrintPreviewTabController() {}

// static
PrintPreviewTabController* PrintPreviewTabController::GetInstance() {
  if (!g_browser_process)
    return NULL;
  return g_browser_process->print_preview_tab_controller();
}

// static
void PrintPreviewTabController::PrintPreview(TabContentsWrapper* tab) {
  if (tab->tab_contents()->showing_interstitial_page())
    return;

  PrintPreviewTabController* tab_controller = GetInstance();
  if (!tab_controller)
    return;
  tab_controller->GetOrCreatePreviewTab(tab);
}

TabContentsWrapper* PrintPreviewTabController::GetOrCreatePreviewTab(
    TabContentsWrapper* initiator_tab) {
  DCHECK(initiator_tab);

  // Get the print preview tab for |initiator_tab|.
  TabContentsWrapper* preview_tab = GetPrintPreviewForTab(initiator_tab);
  if (!preview_tab)
    return CreatePrintPreviewTab(initiator_tab);

  // Show the initiator tab holding the existing preview tab.
  static_cast<RenderViewHostDelegate*>(
      initiator_tab->tab_contents())->Activate();
  return preview_tab;
}

TabContentsWrapper* PrintPreviewTabController::GetPrintPreviewForTab(
    TabContentsWrapper* tab) const {
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

void PrintPreviewTabController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      OnRendererProcessClosed(
          content::Source<content::RenderProcessHost>(source).ptr());
      break;
    }
    case content::NOTIFICATION_TAB_CONTENTS_DESTROYED: {
      TabContents* tab = content::Source<TabContents>(source).ptr();
      TabContentsWrapper* wrapper =
          TabContentsWrapper::GetCurrentWrapperForContents(tab);
      OnTabContentsDestroyed(wrapper);
      break;
    }
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED: {
      NavigationController* controller =
          content::Source<NavigationController>(source).ptr();
      TabContentsWrapper* wrapper =
          TabContentsWrapper::GetCurrentWrapperForContents(
              controller->tab_contents());
      content::LoadCommittedDetails* load_details =
          content::Details<content::LoadCommittedDetails>(details).ptr();
      OnNavEntryCommitted(wrapper, load_details);
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void PrintPreviewTabController::OnRendererProcessClosed(
    content::RenderProcessHost* rph) {
  // Store tabs in a vector and deal with them after iterating through
  // |preview_tab_map_| because RemoveFooTab() can change |preview_tab_map_|.
  std::vector<TabContentsWrapper*> closed_initiator_tabs;
  std::vector<TabContentsWrapper*> closed_preview_tabs;
  for (PrintPreviewTabMap::iterator iter = preview_tab_map_.begin();
       iter != preview_tab_map_.end(); ++iter) {
    TabContentsWrapper* preview_tab = iter->first;
    TabContentsWrapper* initiator_tab = iter->second;
    if (preview_tab->render_view_host()->process() == rph) {
      closed_preview_tabs.push_back(preview_tab);
    } else if (initiator_tab &&
               initiator_tab->render_view_host()->process() == rph) {
      closed_initiator_tabs.push_back(initiator_tab);
    }
  }

  for (size_t i = 0; i < closed_preview_tabs.size(); ++i) {
    RemovePreviewTab(closed_preview_tabs[i]);
    PrintPreviewUI* print_preview_ui =
        static_cast<PrintPreviewUI*>(closed_preview_tabs[i]->web_ui());
    if (print_preview_ui)
      print_preview_ui->OnPrintPreviewTabClosed();
  }

  for (size_t i = 0; i < closed_initiator_tabs.size(); ++i)
    RemoveInitiatorTab(closed_initiator_tabs[i], false);
}

void PrintPreviewTabController::OnTabContentsDestroyed(
    TabContentsWrapper* tab) {
  TabContentsWrapper* preview_tab = GetPrintPreviewForTab(tab);
  if (!preview_tab) {
    NOTREACHED();
    return;
  }

  if (tab == preview_tab)
    RemovePreviewTab(tab);
  else
    RemoveInitiatorTab(tab, false);
}

void PrintPreviewTabController::OnNavEntryCommitted(
    TabContentsWrapper* tab, content::LoadCommittedDetails* details) {
  TabContentsWrapper* preview_tab = GetPrintPreviewForTab(tab);
  if (!preview_tab) {
    NOTREACHED();
    return;
  }
  bool source_tab_is_preview_tab = (tab == preview_tab);

  if (source_tab_is_preview_tab) {
    // Preview tab navigated.
    if (details) {
      content::PageTransition transition_type =
          details->entry->transition_type();
      content::NavigationType nav_type = details->type;

      // New |preview_tab| is created. Don't update/erase map entry.
      if (waiting_for_new_preview_page_ &&
          transition_type == content::PAGE_TRANSITION_START_PAGE &&
          nav_type == content::NAVIGATION_TYPE_NEW_PAGE) {
        waiting_for_new_preview_page_ = false;
        SetInitiatorTabURLAndTitle(preview_tab);

        // Disabling the delegate will prevent all future navigation.
        tab->tab_contents()->set_delegate(NULL);
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

  // Initiator tab navigated.
  RemoveInitiatorTab(tab, true);
}

// static
bool PrintPreviewTabController::IsPrintPreviewTab(TabContentsWrapper* tab) {
  return IsPrintPreviewURL(tab->tab_contents()->GetURL());
}

// static
bool PrintPreviewTabController::IsPrintPreviewURL(const GURL& url) {
  return (url.SchemeIs(chrome::kChromeUIScheme) &&
          url.host() == chrome::kChromeUIPrintHost);
}

void PrintPreviewTabController::EraseInitiatorTabInfo(
    TabContentsWrapper* preview_tab) {
  PrintPreviewTabMap::iterator it = preview_tab_map_.find(preview_tab);
  if (it == preview_tab_map_.end())
    return;

  RemoveObservers(it->second);
  preview_tab_map_[preview_tab] = NULL;
}

bool PrintPreviewTabController::is_creating_print_preview_tab() const {
  return is_creating_print_preview_tab_;
}

TabContentsWrapper* PrintPreviewTabController::GetInitiatorTab(
    TabContentsWrapper* preview_tab) {
  PrintPreviewTabMap::iterator it = preview_tab_map_.find(preview_tab);
  if (it != preview_tab_map_.end())
    return preview_tab_map_[preview_tab];
  return NULL;
}

TabContentsWrapper* PrintPreviewTabController::CreatePrintPreviewTab(
    TabContentsWrapper* initiator_tab) {
  AutoReset<bool> auto_reset(&is_creating_print_preview_tab_, true);
  Browser* current_browser = BrowserList::FindBrowserWithID(
      initiator_tab->restore_tab_helper()->window_id().id());
  if (!current_browser) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame)) {
      Profile* profile = Profile::FromBrowserContext(
          initiator_tab->tab_contents()->browser_context());
      current_browser = Browser::CreateForType(Browser::TYPE_POPUP, profile);
      if (!current_browser) {
        NOTREACHED() << "Failed to create popup browser window";
        return NULL;
      }
    } else {
      return NULL;
    }
  }

  HtmlDialogUIDelegate* delegate = new PrintPreviewTabDelegate(initiator_tab);
  ConstrainedHtmlUIDelegate* html_delegate =
      ConstrainedHtmlUI::CreateConstrainedHtmlDialog(current_browser->profile(),
                                                     delegate,
                                                     initiator_tab);
  TabContentsWrapper* preview_tab = html_delegate->tab();
  EnableInternalPDFPluginForTab(preview_tab);

  // Add an entry to the map.
  preview_tab_map_[preview_tab] = initiator_tab;
  waiting_for_new_preview_page_ = true;

  AddObservers(initiator_tab);
  AddObservers(preview_tab);

  return preview_tab;
}

void PrintPreviewTabController::SetInitiatorTabURLAndTitle(
    TabContentsWrapper* preview_tab) {
  TabContentsWrapper* initiator_tab = GetInitiatorTab(preview_tab);
  if (initiator_tab && preview_tab->web_ui()) {
    PrintPreviewUI* print_preview_ui =
        static_cast<PrintPreviewUI*>(preview_tab->web_ui());
    print_preview_ui->SetInitiatorTabURLAndTitle(
        initiator_tab->tab_contents()->GetURL().spec(),
        initiator_tab->print_view_manager()->RenderSourceName());
  }
}

void PrintPreviewTabController::AddObservers(TabContentsWrapper* tab) {
  TabContents* contents = tab->tab_contents();
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                 content::Source<TabContents>(contents));
  registrar_.Add(
      this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&contents->controller()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  content::RenderProcessHost* rph = tab->render_view_host()->process();
  if (!registrar_.IsRegistered(this,
                               content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                               content::Source<content::RenderProcessHost>(
                                  rph))) {
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   content::Source<content::RenderProcessHost>(rph));
  }
}

void PrintPreviewTabController::RemoveObservers(TabContentsWrapper* tab) {
  TabContents* contents = tab->tab_contents();
  registrar_.Remove(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                    content::Source<TabContents>(contents));
  registrar_.Remove(
      this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<NavigationController>(&contents->controller()));

  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  content::RenderProcessHost* rph = tab->render_view_host()->process();
  if (registrar_.IsRegistered(this,
                              content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                              content::Source<content::RenderProcessHost>(
                                  rph))) {
    registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                      content::Source<content::RenderProcessHost>(rph));
  }
}

void PrintPreviewTabController::RemoveInitiatorTab(
    TabContentsWrapper* initiator_tab, bool is_navigation) {
  TabContentsWrapper* preview_tab = GetPrintPreviewForTab(initiator_tab);
  DCHECK(preview_tab);
  // Update the map entry first, so when the print preview tab gets destroyed
  // and reaches RemovePreviewTab(), it does not attempt to also remove the
  // initiator tab's observers.
  preview_tab_map_[preview_tab] = NULL;
  RemoveObservers(initiator_tab);

  // For the navigation case, PrintPreviewDone() has already been called in
  // PrintPreviewMessageHandler::NavigateToPendingEntry().
  if (!is_navigation)
    initiator_tab->print_view_manager()->PrintPreviewDone();

  // Initiator tab is closed. Close the print preview tab too.
  PrintPreviewUI* print_preview_ui =
      static_cast<PrintPreviewUI*>(preview_tab->web_ui());
  if (print_preview_ui)
    print_preview_ui->OnInitiatorTabClosed();
}

void PrintPreviewTabController::RemovePreviewTab(
    TabContentsWrapper* preview_tab) {
  // Remove the initiator tab's observers before erasing the mapping.
  TabContentsWrapper* initiator_tab = GetInitiatorTab(preview_tab);
  if (initiator_tab) {
    RemoveObservers(initiator_tab);
    initiator_tab->print_view_manager()->PrintPreviewDone();
  }

  // Print preview TabContents is destroyed. Notify |PrintPreviewUI| to abort
  // the initiator tab preview request.
  PrintPreviewUI* print_preview_ui =
      static_cast<PrintPreviewUI*>(preview_tab->web_ui());
  if (print_preview_ui)
    print_preview_ui->OnTabDestroyed();

  preview_tab_map_.erase(preview_tab);
  RemoveObservers(preview_tab);
}

}  // namespace printing
