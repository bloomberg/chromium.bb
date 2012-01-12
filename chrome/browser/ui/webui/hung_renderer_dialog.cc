// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/hung_renderer_dialog.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/dialog_style.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {
HungRendererDialog* g_instance = NULL;
const int kHungRendererDialogWidth = 425;
const int kHungRendererDialogHeight = 200;
}

namespace browser {

void ShowHungRendererDialog(WebContents* contents) {
  // TODO(jhawkins): move this into a non-webui location.
#if defined(USE_AURA)
  ShowNativeHungRendererDialog(contents);
#elif defined(OS_CHROMEOS)
  HungRendererDialog::ShowHungRendererDialog(contents);
#else
  // TODO(rbyers): Remove IsMoreWebUI check once we decide for sure which
  // platforms will use the WebUI version of this dialog.
  if (chrome_web_ui::IsMoreWebUI())
    HungRendererDialog::ShowHungRendererDialog(contents);
  else
    ShowNativeHungRendererDialog(contents);
#endif
}

void HideHungRendererDialog(WebContents* contents) {
#if defined(USE_AURA)
  HideNativeHungRendererDialog(contents);
#elif defined(OS_CHROMEOS)
  HungRendererDialog::HideHungRendererDialog(contents);
#else
  if (chrome_web_ui::IsMoreWebUI())
    HungRendererDialog::HideHungRendererDialog(contents);
  else
    HideNativeHungRendererDialog(contents);
#endif
}

}  // namespace browser

////////////////////////////////////////////////////////////////////////////////
// HungRendererDialog public static methods

void HungRendererDialog::ShowHungRendererDialog(WebContents* contents) {
  ShowHungRendererDialogInternal(contents, true);
}

void HungRendererDialog::HideHungRendererDialog(WebContents* contents) {
  if (!logging::DialogsAreSuppressed() && g_instance)
    g_instance->HideDialog(contents);
}

////////////////////////////////////////////////////////////////////////////////
// HungRendererDialog::WebContentsObserverImpl

HungRendererDialog::WebContentsObserverImpl::WebContentsObserverImpl(
    HungRendererDialog* dialog,
    WebContents* contents)
    : content::WebContentsObserver(contents),
      dialog_(dialog) {
}

void HungRendererDialog::WebContentsObserverImpl::RenderViewGone(
    base::TerminationStatus status) {
  dialog_->HideDialog(web_contents());
}

void HungRendererDialog::WebContentsObserverImpl::WebContentsDestroyed(
    WebContents* tab) {
  dialog_->HideDialog(tab);
}

////////////////////////////////////////////////////////////////////////////////
// HungRendererDialog private methods

HungRendererDialog::HungRendererDialog(bool is_enabled)
    : contents_(NULL),
      handler_(NULL),
      is_enabled_(is_enabled),
      window_(NULL) {
}

HungRendererDialog::~HungRendererDialog() {
}

void HungRendererDialog::ShowHungRendererDialogInternal(WebContents* contents,
                                                        bool is_enabled) {
  if (!logging::DialogsAreSuppressed()) {
    if (g_instance)
      return;
    g_instance = new HungRendererDialog(is_enabled);
    g_instance->ShowDialog(contents);
  }
}

void HungRendererDialog::ShowDialog(WebContents* contents) {
  DCHECK(contents);
  contents_ = contents;
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  handler_ = new HungRendererDialogHandler(contents_);
  window_ = browser->BrowserShowHtmlDialog(this, NULL, STYLE_GENERIC);
  contents_observer_.reset(new WebContentsObserverImpl(this, contents_));
}

void HungRendererDialog::HideDialog(WebContents* contents) {
  DCHECK(contents);
  // Don't close the dialog if it's a WebContents for some other renderer.
  if (contents_ && contents_->GetRenderProcessHost() !=
      contents->GetRenderProcessHost())
    return;
  // Settings |contents_| to NULL prevents the hang monitor from restarting.
  // We do this because the close dialog handler runs whether it is trigged by
  // the user closing the box, or by being closed externally with widget->Close.
  contents_observer_.reset();
  contents_ = NULL;
  DCHECK(handler_);
  handler_->CloseDialog();
}

ui::ModalType HungRendererDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_NONE;
}

string16 HungRendererDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_TITLE);
}

GURL HungRendererDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIHungRendererDialogURL);
}

void HungRendererDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(handler_);
}

void HungRendererDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kHungRendererDialogWidth, kHungRendererDialogHeight);
}

std::string HungRendererDialog::GetDialogArgs() const {
  return std::string();  // There are no args used.
}

void HungRendererDialog::OnDialogClosed(const std::string& json_retval) {
  if (is_enabled_) {
    // Figure out what the response was.
    scoped_ptr<Value> root(base::JSONReader::Read(json_retval, false));
    bool response = false;
    ListValue* list = NULL;
    // If the dialog closes because of a button click then the json is a list
    // containing a single bool.  If the dialog closes some other way, then we
    // assume it means no permission was given to kill tabs.
    if (root.get() && root->GetAsList(&list) && list &&
        list->GetBoolean(0, &response) && response) {
      // The user indicated that it is OK to kill the renderer process.
      if (contents_ && contents_->GetRenderProcessHost()) {
        base::KillProcess(contents_->GetRenderProcessHost()->GetHandle(),
                          content::RESULT_CODE_HUNG, false);
      }
    } else {
      // No indication from the user that it is ok to kill anything. Just wait.
      // Start waiting again for responsiveness.
      if (contents_ && contents_->GetRenderViewHost())
        contents_->GetRenderViewHost()->RestartHangMonitorTimeout();
    }
  }
  g_instance = NULL;
  delete this;
}

void HungRendererDialog::OnCloseContents(WebContents* source,
                                         bool* out_close_dialog) {
  NOTIMPLEMENTED();
}

bool HungRendererDialog::ShouldShowDialogTitle() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// HungRendererDialogHandler methods

HungRendererDialogHandler::HungRendererDialogHandler(
    WebContents* contents)
  : contents_(contents) {
}

void HungRendererDialogHandler::CloseDialog() {
  static_cast<HtmlDialogUI*>(web_ui())->CloseDialog(NULL);
}

void HungRendererDialogHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestTabContentsList",
      base::Bind(&HungRendererDialogHandler::RequestTabContentsList,
                 base::Unretained(this)));
}

void HungRendererDialogHandler::RequestTabContentsList(
    const base::ListValue* args) {
  ListValue tab_contents_list;
  for (TabContentsIterator it; !it.done(); ++it) {
    if (it->web_contents()->GetRenderProcessHost() ==
        contents_->GetRenderProcessHost()) {
      string16 title = it->web_contents()->GetTitle();
      if (title.empty())
        title = CoreTabHelper::GetDefaultTitle();
      // Add details for |url| and |title|.
      DictionaryValue* dict = new DictionaryValue();
      dict->SetString("url", it->web_contents()->GetURL().spec());
      dict->SetString("title", title);
      tab_contents_list.Append(dict);
    }
  }
  // Send list of tab contents details to javascript.
  web_ui()->CallJavascriptFunction("hungRendererDialog.setTabContentsList",
                                   tab_contents_list);
}
