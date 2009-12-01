// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/temp_scaffolding_stubs.h"

#include <vector>

#include "base/gfx/rect.h"
#include "base/logging.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/fonts_languages_window.h"
#include "chrome/browser/memory_details.h"

//--------------------------------------------------------------------------

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

void InstallJankometer(const CommandLine&) {
  // http://code.google.com/p/chromium/issues/detail?id=8077
}

void UninstallJankometer() {
  // http://code.google.com/p/chromium/issues/detail?id=8077
}

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

//------------------------------------------------------------------------------

void ShowFontsLanguagesWindow(gfx::NativeWindow window,
                              FontsLanguagesPage page,
                              Profile* profile) {
  NOTIMPLEMENTED();
}
