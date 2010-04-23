// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/chrome_views_delegate.h"

#include "app/clipboard/clipboard.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/window_sizer.h"
#include "gfx/rect.h"

#if defined(OS_WIN)
#include "chrome/browser/app_icon_win.h"
#endif

///////////////////////////////////////////////////////////////////////////////
// ChromeViewsDelegate, views::ViewsDelegate implementation:

Clipboard* ChromeViewsDelegate::GetClipboard() const {
  return g_browser_process->clipboard();
}

void ChromeViewsDelegate::SaveWindowPlacement(const std::wstring& window_name,
                                              const gfx::Rect& bounds,
                                              bool maximized) {
  if (!g_browser_process->local_state())
    return;

  DictionaryValue* window_preferences =
      g_browser_process->local_state()->GetMutableDictionary(
          window_name.c_str());
  window_preferences->SetInteger(L"left", bounds.x());
  window_preferences->SetInteger(L"top", bounds.y());
  window_preferences->SetInteger(L"right", bounds.right());
  window_preferences->SetInteger(L"bottom", bounds.bottom());
  window_preferences->SetBoolean(L"maximized", maximized);

  scoped_ptr<WindowSizer::MonitorInfoProvider> monitor_info_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  gfx::Rect work_area(
      monitor_info_provider->GetMonitorWorkAreaMatching(bounds));
  window_preferences->SetInteger(L"work_area_left", work_area.x());
  window_preferences->SetInteger(L"work_area_top", work_area.y());
  window_preferences->SetInteger(L"work_area_right", work_area.right());
  window_preferences->SetInteger(L"work_area_bottom", work_area.bottom());
}

bool ChromeViewsDelegate::GetSavedWindowBounds(const std::wstring& window_name,
                                               gfx::Rect* bounds) const {
  if (!g_browser_process->local_state())
    return false;

  const DictionaryValue* dictionary =
      g_browser_process->local_state()->GetDictionary(window_name.c_str());
  int left, top, right, bottom;
  if (!dictionary || !dictionary->GetInteger(L"left", &left) ||
      !dictionary->GetInteger(L"top", &top) ||
      !dictionary->GetInteger(L"right", &right) ||
      !dictionary->GetInteger(L"bottom", &bottom))
    return false;

  bounds->SetRect(left, top, right - left, bottom - top);
  return true;
}

bool ChromeViewsDelegate::GetSavedMaximizedState(
    const std::wstring& window_name,
    bool* maximized) const {
  if (!g_browser_process->local_state())
    return false;

  const DictionaryValue* dictionary =
      g_browser_process->local_state()->GetDictionary(window_name.c_str());
  return dictionary && dictionary->GetBoolean(L"maximized", maximized) &&
      maximized;
}

#if defined(OS_WIN)
HICON ChromeViewsDelegate::GetDefaultWindowIcon() const {
  return GetAppIcon();
}
#endif

void ChromeViewsDelegate::AddRef() {
  g_browser_process->AddRefModule();
}

void ChromeViewsDelegate::ReleaseRef() {
  g_browser_process->ReleaseModule();
}
