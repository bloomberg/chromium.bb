// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cloud_print_signin_dialog.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/dialog_style.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui_message_handler.h"

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;
using content::WebUIMessageHandler;

// This module implements a sign in dialog for cloud print.
// it is based heavily off "chrome/browser/printing/print_dialog_cloud.cc".
// See the comments in that file for a discussion about how this works.

namespace cloud_print_signin_dialog {

// The flow handler sends our dialog to the correct URL, saves size info,
// and closes the dialog when sign in is complete.
class CloudPrintSigninFlowHandler : public WebUIMessageHandler,
                                    public content::NotificationObserver {
 public:
  explicit CloudPrintSigninFlowHandler(WebContents* parent_tab);
  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
 private:
  // Records the final size of the dialog in prefs.
  void StoreDialogSize();
  content::NotificationRegistrar registrar_;
  WebContents* parent_tab_;
};

CloudPrintSigninFlowHandler::CloudPrintSigninFlowHandler(
    WebContents* parent_tab) : parent_tab_(parent_tab) {
}

void CloudPrintSigninFlowHandler::RegisterMessages() {
  if (web_ui() && web_ui()->GetWebContents()) {
    NavigationController* controller =
        &web_ui()->GetWebContents()->GetController();
    NavigationEntry* pending_entry = controller->GetPendingEntry();
    if (pending_entry)
      pending_entry->SetURL(CloudPrintURL(
          Profile::FromWebUI(web_ui())).GetCloudPrintSigninURL());
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   content::Source<NavigationController>(controller));
  }
}

void CloudPrintSigninFlowHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    GURL url = web_ui()->GetWebContents()->GetURL();
    GURL dialog_url = CloudPrintURL(
        Profile::FromWebUI(web_ui())).GetCloudPrintServiceURL();
    if (url.host() == dialog_url.host() &&
        url.path() == dialog_url.path() &&
        url.scheme() == dialog_url.scheme()) {
      StoreDialogSize();
      web_ui()->GetWebContents()->GetRenderViewHost()->ClosePage();
      static_cast<PrintPreviewUI*>(
          parent_tab_->GetWebUI()->GetController())->OnReloadPrintersList();
    }
  }
}

void CloudPrintSigninFlowHandler::StoreDialogSize() {
  if (web_ui() &&
      web_ui()->GetWebContents() &&
      web_ui()->GetWebContents()->GetView()) {
    gfx::Size size = web_ui()->GetWebContents()->GetView()->GetContainerSize();
    Profile* profile = Profile::FromWebUI(web_ui());
    profile->GetPrefs()->SetInteger(prefs::kCloudPrintSigninDialogWidth,
                                    size.width());
    profile->GetPrefs()->SetInteger(
        prefs::kCloudPrintSigninDialogHeight, size.height());
  }
}

// The delegate provides most of the basic setup for the dialog.
class CloudPrintSigninDelegate : public HtmlDialogUIDelegate {
 public:
  explicit CloudPrintSigninDelegate(WebContents* parent_tab);
  virtual ui::ModalType GetDialogModalType() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(
      WebContents* source, bool* out_close_dialog) OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
 private:
  WebContents* parent_tab_;
};

CloudPrintSigninDelegate::CloudPrintSigninDelegate(WebContents* parent_tab)
    : parent_tab_(parent_tab) {
}

ui::ModalType CloudPrintSigninDelegate::GetDialogModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

string16 CloudPrintSigninDelegate::GetDialogTitle() const {
  return string16();
}

GURL CloudPrintSigninDelegate::GetDialogContentURL() const {
  return GURL(chrome::kChromeUICloudPrintResourcesURL);
}

void CloudPrintSigninDelegate::GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(new CloudPrintSigninFlowHandler(parent_tab_));
}

void CloudPrintSigninDelegate::GetDialogSize(gfx::Size* size) const {
  PrefService* pref_service =
      BrowserList::GetLastActive()->GetProfile()->GetPrefs();
  if (!pref_service->FindPreference(prefs::kCloudPrintSigninDialogWidth)) {
    pref_service->RegisterIntegerPref(prefs::kCloudPrintSigninDialogWidth,
                                      800,
                                      PrefService::UNSYNCABLE_PREF);
  }
  if (!pref_service->FindPreference(prefs::kCloudPrintSigninDialogHeight)) {
    pref_service->RegisterIntegerPref(prefs::kCloudPrintSigninDialogHeight,
                                      600,
                                      PrefService::UNSYNCABLE_PREF);
  }

  size->set_width(
      pref_service->GetInteger(prefs::kCloudPrintSigninDialogWidth));
  size->set_height(
      pref_service->GetInteger(prefs::kCloudPrintSigninDialogHeight));
}

std::string CloudPrintSigninDelegate::GetDialogArgs() const {
  return std::string();
}

void CloudPrintSigninDelegate::OnDialogClosed(const std::string& json_retval) {
}

void CloudPrintSigninDelegate::OnCloseContents(WebContents* source,
                                               bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool CloudPrintSigninDelegate::ShouldShowDialogTitle() const {
  return false;
}

void CreateCloudPrintSigninDialogImpl(int render_process_id,
                                      int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  WebContents* parent_tab =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  if (!parent_tab)
    return;
  HtmlDialogUIDelegate* dialog_delegate =
      new CloudPrintSigninDelegate(parent_tab);
  BrowserList::GetLastActive()->BrowserShowHtmlDialog(dialog_delegate,
                                                      NULL,
                                                      STYLE_GENERIC);
}

void CreateCloudPrintSigninDialog(WebContents* parent_tab) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CreateCloudPrintSigninDialogImpl,
                 parent_tab->GetRenderProcessHost()->GetID(),
                 parent_tab->GetRenderViewHost()->routing_id()));
}
}  // namespace cloud_print_signin_dialog
