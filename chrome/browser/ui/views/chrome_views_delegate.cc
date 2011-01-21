// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/accessibility_event_router_views.h"
#include "chrome/browser/ui/window_sizer.h"
#include "gfx/rect.h"
#include "ui/base/clipboard/clipboard.h"

#if defined(OS_WIN)
#include "chrome/browser/app_icon_win.h"
#endif

///////////////////////////////////////////////////////////////////////////////
// ChromeViewsDelegate, views::ViewsDelegate implementation:

ui::Clipboard* ChromeViewsDelegate::GetClipboard() const {
  return g_browser_process->clipboard();
}

void ChromeViewsDelegate::SaveWindowPlacement(const std::wstring& window_name,
                                              const gfx::Rect& bounds,
                                              bool maximized) {
  if (!g_browser_process->profile_manager())
    return;

  DictionaryValue* window_preferences =
      g_browser_process->profile_manager()->GetDefaultProfile()->GetPrefs()->
          GetMutableDictionary(WideToUTF8(window_name).c_str());
  window_preferences->SetInteger("left", bounds.x());
  window_preferences->SetInteger("top", bounds.y());
  window_preferences->SetInteger("right", bounds.right());
  window_preferences->SetInteger("bottom", bounds.bottom());
  window_preferences->SetBoolean("maximized", maximized);

  scoped_ptr<WindowSizer::MonitorInfoProvider> monitor_info_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  gfx::Rect work_area(
      monitor_info_provider->GetMonitorWorkAreaMatching(bounds));
  window_preferences->SetInteger("work_area_left", work_area.x());
  window_preferences->SetInteger("work_area_top", work_area.y());
  window_preferences->SetInteger("work_area_right", work_area.right());
  window_preferences->SetInteger("work_area_bottom", work_area.bottom());
}

bool ChromeViewsDelegate::GetSavedWindowBounds(const std::wstring& window_name,
                                               gfx::Rect* bounds) const {
  if (!g_browser_process->profile_manager())
    return false;

  const DictionaryValue* dictionary =
      g_browser_process->profile_manager()->GetDefaultProfile()->GetPrefs()->
          GetDictionary(WideToUTF8(window_name).c_str());
  int left, top, right, bottom;
  if (!dictionary || !dictionary->GetInteger("left", &left) ||
      !dictionary->GetInteger("top", &top) ||
      !dictionary->GetInteger("right", &right) ||
      !dictionary->GetInteger("bottom", &bottom))
    return false;

  bounds->SetRect(left, top, right - left, bottom - top);
  return true;
}

bool ChromeViewsDelegate::GetSavedMaximizedState(
    const std::wstring& window_name,
    bool* maximized) const {
  if (!g_browser_process->profile_manager())
    return false;

  const DictionaryValue* dictionary =
      g_browser_process->profile_manager()->GetDefaultProfile()->GetPrefs()->
          GetDictionary(WideToUTF8(window_name).c_str());

  return dictionary && dictionary->GetBoolean("maximized", maximized) &&
      maximized;
}

void ChromeViewsDelegate::NotifyAccessibilityEvent(
    views::View* view, AccessibilityTypes::Event event_type) {
  AccessibilityEventRouterViews::GetInstance()->HandleAccessibilityEvent(
      view, event_type);
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
