// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/hung_renderer_dialog.h"

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/result_codes.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
HungRendererDialog* g_instance = NULL;
const int kHungRendererDialogWidth = 425;
const int kHungRendererDialogHeight = 200;
}


namespace browser {

void ShowHungRendererDialog(TabContents* contents) {
  if (!logging::DialogsAreSuppressed()) {
    if (!g_instance) {
      g_instance = new HungRendererDialog();
    } else {
      NOTIMPLEMENTED() << " ShowHungRendererDialog called twice.";
      return;
    }
    g_instance->ShowDialog(NULL, contents);
  }
}

void HideHungRendererDialog(TabContents* contents) {
  if (!logging::DialogsAreSuppressed() && g_instance) {
    // TODO(wyck): Hide the webui hung renderer dialog.
    NOTIMPLEMENTED() << " TODO: Hide the webui hung renderer dialog.";
  }
}

}  // namespace browser

HungRendererDialog::HungRendererDialog()
    : contents_(NULL) {
}

void HungRendererDialog::ShowDialog(gfx::NativeWindow owning_window,
                                    TabContents* contents) {
  DCHECK(contents);
  contents_ = contents;
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  browser->BrowserShowHtmlDialog(this, owning_window);
}

bool HungRendererDialog::IsDialogModal() const {
  return false;
}

string16 HungRendererDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_TITLE);
}

GURL HungRendererDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIHungRendererDialogURL);
}

void HungRendererDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  // Do nothing. There are no handlers.
}

void HungRendererDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kHungRendererDialogWidth, kHungRendererDialogHeight);
}

std::string HungRendererDialog::GetDialogArgs() const {
  return std::string();  // There are no args used.
}

void HungRendererDialog::OnDialogClosed(const std::string& json_retval) {
  // Figure out what the response was.
  scoped_ptr<Value> root(base::JSONReader::Read(json_retval, false));
  bool response = false;
  ListValue* list;
  if (!root.get() || !root->GetAsList(&list) || !list ||
      !list->GetBoolean(0, &response)) {
    NOTREACHED() << "json does not describe a valid result";
  }

  if (response) {
    // The user indicated that it is OK to kill the renderer process.
    if (contents_ && contents_->GetRenderProcessHost()) {
      base::KillProcess(contents_->GetRenderProcessHost()->GetHandle(),
                        content::RESULT_CODE_HUNG, false);
    }
  } else {
    // No indication from the user that it is ok to kill anything. Just wait.
    // Start waiting again for responsiveness.
    if (contents_ && contents_->render_view_host())
      contents_->render_view_host()->RestartHangMonitorTimeout();
  }

  g_instance = NULL;
  delete this;
}

void HungRendererDialog::OnCloseContents(TabContents* source,
                     bool* out_close_dialog) {
  NOTIMPLEMENTED();
}

bool HungRendererDialog::ShouldShowDialogTitle() const {
  return true;
}
