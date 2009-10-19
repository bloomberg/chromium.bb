// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/temp_scaffolding_stubs.h"

#include <vector>

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/rlz/rlz.h"

#if defined(OS_LINUX)
#include "chrome/browser/dock_info.h"
#include "chrome/common/render_messages.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/fonts_languages_window.h"
#include "chrome/browser/memory_details.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/download/download_request_manager.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#endif

class TabContents;

//--------------------------------------------------------------------------

#if defined(OS_MACOSX)
void AutomationProvider::GetAutocompleteEditForBrowser(
    int browser_handle,
    bool* success,
    int* autocomplete_edit_handle) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetAutocompleteEditText(int autocomplete_edit_handle,
                                                 bool* success,
                                                 std::wstring* text) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::SetAutocompleteEditText(int autocomplete_edit_handle,
                                                 const std::wstring& text,
                                                 bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::AutocompleteEditGetMatches(
    int autocomplete_edit_handle,
    bool* success,
    std::vector<AutocompleteMatchData>* matches) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::AutocompleteEditIsQueryInProgress(
    int autocomplete_edit_handle,
    bool* success,
    bool* query_in_progress) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::OnMessageFromExternalHost(
    int handle, const std::string& message, const std::string& origin,
    const std::string& target) {
  NOTIMPLEMENTED();
}

#endif  // defined(OS_MACOSX)

//--------------------------------------------------------------------------

#if defined(OS_MACOSX)
// static
bool FirstRun::ProcessMasterPreferences(const FilePath& user_data_dir,
                                        const FilePath& master_prefs_path,
                                        std::vector<std::wstring>* new_tabs,
                                        int* ping_delay,
                                        bool* homepage_defined,
                                        int* do_import_items,
                                        int* dont_import_items) {
  // http://code.google.com/p/chromium/issues/detail?id=11971
  // Pretend we processed them correctly.
  return true;
}

// static
int FirstRun::ImportNow(Profile* profile, const CommandLine& cmdline) {
  // http://code.google.com/p/chromium/issues/detail?id=11971
  return 0;
}
#endif

bool FirstRun::CreateChromeDesktopShortcut() {
  NOTIMPLEMENTED();
  return false;
}

bool FirstRun::CreateChromeQuickLaunchShortcut() {
  NOTIMPLEMENTED();
  return false;
}

// static
bool Upgrade::IsBrowserAlreadyRunning() {
  // http://code.google.com/p/chromium/issues/detail?id=9295
  return false;
}

// static
bool Upgrade::RelaunchChromeBrowser(const CommandLine& command_line) {
  // http://code.google.com/p/chromium/issues/detail?id=9295
  return true;
}

// static
bool Upgrade::SwapNewChromeExeIfPresent() {
  // http://code.google.com/p/chromium/issues/detail?id=9295
  return true;
}

// static
Upgrade::TryResult ShowTryChromeDialog(size_t version) {
  return Upgrade::TD_NOT_NOW;
}

//--------------------------------------------------------------------------

#if defined(OS_MACOSX)
void InstallJankometer(const CommandLine&) {
  // http://code.google.com/p/chromium/issues/detail?id=8077
}

void UninstallJankometer() {
  // http://code.google.com/p/chromium/issues/detail?id=8077
}
#endif

//--------------------------------------------------------------------------

void RLZTracker::CleanupRlz() {
  // http://code.google.com/p/chromium/issues/detail?id=8152
}

bool RLZTracker::GetAccessPointRlz(AccessPoint point, std::wstring* rlz) {
  // http://code.google.com/p/chromium/issues/detail?id=8152
  return false;
}

bool RLZTracker::RecordProductEvent(Product product, AccessPoint point,
                                    Event event) {
  // http://code.google.com/p/chromium/issues/detail?id=8152
  return false;
}

//--------------------------------------------------------------------------

#if defined(OS_MACOSX)
MemoryDetails::MemoryDetails() {
  NOTIMPLEMENTED();
  process_data_.push_back(ProcessData());
}

void MemoryDetails::StartFetch() {
  NOTIMPLEMENTED();

  // Other implementations implicitly own the object by passing it to
  // IO and UI tasks.  This code is called from AboutMemoryHandler's
  // constructor, so there is no reference to Release(), yet.
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableMethod(this, &MemoryDetails::OnDetailsAvailable));
}
#endif

#if !defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
namespace download_util {

void DragDownload(const DownloadItem* download,
                  SkBitmap* icon,
                  gfx::NativeView view) {
  NOTIMPLEMENTED();
}

}  // namespace download_util
#endif

#if defined(OS_MACOSX)
void BrowserList::AllBrowsersClosed() {
  // TODO(port): Close any dependent windows if necessary when the last browser
  //             window is closed.
}

//--------------------------------------------------------------------------

bool DockInfo::GetNewWindowBounds(gfx::Rect* new_window_bounds,
                                  bool* maximize_new_window) const {
  // TODO(pinkerton): Implement on Mac.
  // http://crbug.com/9274
  return true;
}

void DockInfo::AdjustOtherWindowBounds() const {
  // TODO(pinkerton): Implement on Mac.
  // http://crbug.com/9274
}
#endif

//------------------------------------------------------------------------------

#if defined(OS_MACOSX)
void ShowFontsLanguagesWindow(gfx::NativeWindow window,
                              FontsLanguagesPage page,
                              Profile* profile) {
  NOTIMPLEMENTED();
}
#endif

//------------------------------------------------------------------------------

#if defined(TOOLKIT_VIEWS)

ConstrainedWindow* ConstrainedWindow::CreateConstrainedDialog(
    TabContents* owner,
    ConstrainedWindowDelegate* delegate) {
  NOTIMPLEMENTED();
  return NULL;
}

#endif
