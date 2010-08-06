// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/setup_flow.h"

#include "app/gfx/font_util.h"
#include "base/callback.h"
#include "base/histogram.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#if defined(OS_MACOSX)
#include "chrome/browser/cocoa/html_dialog_window_controller_cppsafe.h"
#endif
#include "chrome/browser/dom_ui/dom_ui_util.h"
#include "chrome/browser/google_service_auth_error.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "gfx/font.h"
#include "grit/locale_settings.h"

namespace remoting_setup {

// Use static Run method to get an instance.
SetupFlow::SetupFlow(const std::string& args, Profile* profile)
    : dialog_start_args_(args),
      profile_(profile) {
}

SetupFlow::~SetupFlow() {
}

void SetupFlow::GetDialogSize(gfx::Size* size) const {
  PrefService* prefs = profile_->GetPrefs();
  gfx::Font approximate_web_font = gfx::Font::CreateFont(
      UTF8ToWide(prefs->GetString(prefs::kWebKitSansSerifFontFamily)),
      prefs->GetInteger(prefs::kWebKitDefaultFontSize));

  // TODO(pranavk) Replace the following SYNC resources with REMOTING Resources.
  *size = gfx::GetLocalizedContentsSizeForFont(
      IDS_SYNC_SETUP_WIZARD_WIDTH_CHARS,
      IDS_SYNC_SETUP_WIZARD_HEIGHT_LINES,
      approximate_web_font);

#if defined(OS_MACOSX)
  // NOTE(pranavk): This is a hack to work around a problem with font height on
  // windows.  Basically font metrics are incorrectly returned in logical units
  // instead of pixels on Windows.  Logical units are very commonly 96 DPI
  // so our localized char/line counts are too small by a factor of 96/72.
  // So we compensate for this on non-windows platform.
  //
  // TODO(pranavk): Remove this hack once we fix the windows font problem (or at
  // least work around it in some other place).
  float scale_hack = 96.f/72.f;
  size->set_width(size->width() * scale_hack);
  size->set_height(size->height() * scale_hack);
#endif
}

// A callback to notify the delegate that the dialog closed.
void SetupFlow::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

// static
void SetupFlow::GetArgsForGaiaLogin(DictionaryValue* args) {
  // TODO(pranavk): implement this method.
}

void SetupFlow::GetDOMMessageHandlers(
    std::vector<DOMMessageHandler*>* handlers) const {
  // TODO(pranavk): implement this method.
}


void SetupFlow::Advance() {
  // TODO(pranavk): implement this method.
}

void SetupFlow::Focus() {
#if defined(OS_MACOSX)
  if (html_dialog_window_) {
    platform_util::ActivateWindow(html_dialog_window_);
  }
#else
  // TODO(pranavk): We don't currently have a way to get the reference to the
  // dialog on windows/linux.  This can be resolved by a cross platform
  // implementation of HTML dialogs as described below.
  NOTIMPLEMENTED();
#endif  // defined(OS_MACOSX)
}

// static
SetupFlow* SetupFlow::Run(Profile* service) {
  DictionaryValue args;
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  SetupFlow* flow = new SetupFlow(json_args, service);
#if defined(OS_MACOSX)
  // TODO(pranavk): Figure out a cleaner way to do this than to have this
  // gross per-OS behavior, i.e. have a cross-platform ShowHtmlDialog()
  // function that is not tied to a browser instance.
  flow->html_dialog_window_ =
      html_dialog_window_controller::ShowHtmlDialog(
          flow, service->profile());
#else
  Browser* b = BrowserList::GetLastActive();
  if (b) {
    b->BrowserShowHtmlDialog(flow, NULL);
  } else {
    delete flow;
    return NULL;
  }
#endif  // defined(OS_MACOSX)
  return flow;
}

}  // namespace remoting_setup
