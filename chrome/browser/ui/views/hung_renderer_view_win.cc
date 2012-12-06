// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hung_renderer_view_win.h"

#include "base/win/metro.h"
#include "chrome/browser/hang_monitor/hang_crash_dump_win.h"
#include "chrome/browser/ui/views/hung_renderer_view.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "win8/util/win8_util.h"

// Metro functions for displaying and dismissing a dialog box.
typedef void (*MetroShowDialogBox)(
    const wchar_t* title,
    const wchar_t* content,
    const wchar_t* button1_label,
    const wchar_t* button2_label,
    base::win::MetroDialogButtonPressedHandler button1_handler,
    base::win::MetroDialogButtonPressedHandler button2_handler);

typedef void (*MetroDismissDialogBox)();

using content::BrowserThread;
using content::WebContents;

HungRendererDialogMetro* HungRendererDialogMetro::g_instance_ = NULL;

bool PlatformShowCustomHungRendererDialog(WebContents* contents) {
  if (!win8::IsSingleWindowMetroMode())
    return false;

  HungRendererDialogMetro::Create()->Show(contents);
  return true;
}

bool PlatformHideCustomHungRendererDialog(WebContents* contents) {
  if (!win8::IsSingleWindowMetroMode())
    return false;

  if (HungRendererDialogMetro::GetInstance())
    HungRendererDialogMetro::GetInstance()->Hide(contents);
  return true;
}

// static
void HungRendererDialogView::KillRendererProcess(
    base::ProcessHandle process_handle) {
  // Try to generate a crash report for the hung process.
  CrashDumpAndTerminateHungChildProcess(process_handle);
}

// static
HungRendererDialogMetro* HungRendererDialogMetro::Create() {
  if (!GetInstance())
    g_instance_ = new HungRendererDialogMetro;
  return g_instance_;
}

// static
HungRendererDialogMetro* HungRendererDialogMetro::GetInstance() {
  return g_instance_;
}

HungRendererDialogMetro::HungRendererDialogMetro()
    : contents_(NULL),
      metro_dialog_displayed_(false) {
}

HungRendererDialogMetro::~HungRendererDialogMetro() {
  g_instance_ = NULL;
}

void HungRendererDialogMetro::Show(WebContents* contents) {
  if (!metro_dialog_displayed_ &&
      HungRendererDialogView::IsFrameActive(contents)) {
    HMODULE metro_dll = base::win::GetMetroModule();
    DCHECK(metro_dll);
    if (metro_dll) {
      MetroShowDialogBox show_dialog_box = reinterpret_cast<MetroShowDialogBox>
          (::GetProcAddress(metro_dll, "ShowDialogBox"));
      DCHECK(show_dialog_box);
      if (show_dialog_box) {
        string16 dialog_box_title =
            l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_TITLE);

        string16 kill_button_label =
            l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_END);

        string16 wait_button_label =
            l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_WAIT);

        contents_ = contents;
        metro_dialog_displayed_ = true;

        (*show_dialog_box)(dialog_box_title.c_str(),
                           contents->GetTitle().c_str(),
                           kill_button_label.c_str(),
                           wait_button_label.c_str(),
                           HungRendererDialogMetro::OnMetroKillProcess,
                           HungRendererDialogMetro::OnMetroWait);
      }
    }
  }
}

void HungRendererDialogMetro::Hide(WebContents* contents) {
  HMODULE metro_dll = base::win::GetMetroModule();
  DCHECK(metro_dll);
  if (metro_dll) {
    MetroDismissDialogBox dismiss_dialog_box =
        reinterpret_cast<MetroDismissDialogBox>
            (::GetProcAddress(metro_dll, "DismissDialogBox"));
    DCHECK(dismiss_dialog_box);
    if (dismiss_dialog_box) {
      (*dismiss_dialog_box)();
      ResetMetroState();
    }
  }
}

void HungRendererDialogMetro::ResetMetroState() {
  metro_dialog_displayed_ = false;
  contents_ = NULL;
  delete g_instance_;
}

// static
void HungRendererDialogMetro::OnMetroKillProcess() {
  // Metro chrome will invoke these handlers on the metro thread. Ensure that
  // we switch to the UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(HungRendererDialogMetro::OnMetroKillProcess));
    return;
  }

  // Its possible that we got deleted in the meantime.
  if (!GetInstance())
    return;

  DCHECK(GetInstance()->contents_);

  HungRendererDialogView::KillRendererProcess(
      GetInstance()->contents_->GetRenderProcessHost()->GetHandle());

  // The metro dialog box is dismissed when the button handlers are invoked.
  GetInstance()->ResetMetroState();
}

// static
void HungRendererDialogMetro::OnMetroWait() {
  // Metro chrome will invoke these handlers on the metro thread. Ensure that
  // we switch to the UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(HungRendererDialogMetro::OnMetroWait));
    return;
  }

  // Its possible that we got deleted in the meantime.
  if (!GetInstance())
    return;

  GetInstance()->ResetMetroState();
}
