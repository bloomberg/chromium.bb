// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/event_disposition.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/accessibility/accessibility_event_router_views.h"
#include "chrome/common/pref_names.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "chrome/browser/app_icon_win.h"
#endif

#if defined(USE_AURA)
#include "ui/views/widget/desktop_native_widget_helper_aura.h"
#endif

#if defined(USE_ASH)
#include "ash/shell.h"
#include "chrome/browser/ui/ash/ash_init.h"
#endif

namespace {

// If the given window has a profile associated with it, use that profile's
// preference service. Otherwise, store and retrieve the data from Local State.
// This function may return NULL if the necessary pref service has not yet
// been initialized.
// TODO(mirandac): This function will also separate windows by profile in a
// multi-profile environment.
PrefService* GetPrefsForWindow(const views::Widget* window) {
  Profile* profile = reinterpret_cast<Profile*>(
      window->GetNativeWindowProperty(Profile::kProfileKey));
  if (!profile) {
    // Use local state for windows that have no explicit profile.
    return g_browser_process->local_state();
  }
  return profile->GetPrefs();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ChromeViewsDelegate, views::ViewsDelegate implementation:

ui::Clipboard* ChromeViewsDelegate::GetClipboard() const {
  return g_browser_process->clipboard();
}

void ChromeViewsDelegate::SaveWindowPlacement(const views::Widget* window,
                                              const std::string& window_name,
                                              const gfx::Rect& bounds,
                                              ui::WindowShowState show_state) {
  PrefService* prefs = GetPrefsForWindow(window);
  if (!prefs)
    return;

  DCHECK(prefs->FindPreference(window_name.c_str()));
  DictionaryPrefUpdate update(prefs, window_name.c_str());
  DictionaryValue* window_preferences = update.Get();
  window_preferences->SetInteger("left", bounds.x());
  window_preferences->SetInteger("top", bounds.y());
  window_preferences->SetInteger("right", bounds.right());
  window_preferences->SetInteger("bottom", bounds.bottom());
  window_preferences->SetBoolean("maximized",
                                 show_state == ui::SHOW_STATE_MAXIMIZED);
  gfx::Rect work_area(
      gfx::Screen::GetDisplayMatching(bounds).work_area());
  window_preferences->SetInteger("work_area_left", work_area.x());
  window_preferences->SetInteger("work_area_top", work_area.y());
  window_preferences->SetInteger("work_area_right", work_area.right());
  window_preferences->SetInteger("work_area_bottom", work_area.bottom());
}

bool ChromeViewsDelegate::GetSavedWindowPlacement(
    const std::string& window_name,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  PrefService* prefs = g_browser_process->local_state();
  if (!prefs)
    return false;

  DCHECK(prefs->FindPreference(window_name.c_str()));
  const DictionaryValue* dictionary = prefs->GetDictionary(window_name.c_str());
  int left, top, right, bottom;
  if (!dictionary || !dictionary->GetInteger("left", &left) ||
      !dictionary->GetInteger("top", &top) ||
      !dictionary->GetInteger("right", &right) ||
      !dictionary->GetInteger("bottom", &bottom))
    return false;

  bounds->SetRect(left, top, right - left, bottom - top);

  bool maximized = false;
  if (dictionary)
    dictionary->GetBoolean("maximized", &maximized);
  *show_state = maximized ? ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_NORMAL;

  return true;
}

void ChromeViewsDelegate::NotifyAccessibilityEvent(
    views::View* view, ui::AccessibilityTypes::Event event_type) {
  AccessibilityEventRouterViews::GetInstance()->HandleAccessibilityEvent(
      view, event_type);
}

void ChromeViewsDelegate::NotifyMenuItemFocused(const string16& menu_name,
                                                const string16& menu_item_name,
                                                int item_index,
                                                int item_count,
                                                bool has_submenu) {
  AccessibilityEventRouterViews::GetInstance()->HandleMenuItemFocused(
      menu_name, menu_item_name, item_index, item_count, has_submenu);
}

#if defined(OS_WIN)
HICON ChromeViewsDelegate::GetDefaultWindowIcon() const {
  return GetAppIcon();
}
#endif

views::NonClientFrameView* ChromeViewsDelegate::CreateDefaultNonClientFrameView(
    views::Widget* widget) {
#if defined(USE_ASH)
  return ash::Shell::GetInstance()->CreateDefaultNonClientFrameView(widget);
#else
  return NULL;
#endif
}

bool ChromeViewsDelegate::UseTransparentWindows() const {
#if defined(USE_ASH)
  // Ash uses transparent window frames above.
  return true;
#else
  return false;
#endif
}

void ChromeViewsDelegate::AddRef() {
  g_browser_process->AddRefModule();
}

void ChromeViewsDelegate::ReleaseRef() {
  g_browser_process->ReleaseModule();
}

int ChromeViewsDelegate::GetDispositionForEvent(int event_flags) {
  return chrome::DispositionFromEventFlags(event_flags);
}

#if defined(USE_AURA)
views::NativeWidgetHelperAura* ChromeViewsDelegate::CreateNativeWidgetHelper(
    views::NativeWidgetAura* native_widget) {
  // TODO(beng): insufficient but currently necessary. http://crbug.com/133312
#if defined(USE_ASH)
  if (!chrome::ShouldOpenAshOnStartup())
#endif
    return new views::DesktopNativeWidgetHelperAura(native_widget);
#if defined(USE_ASH)
  return NULL;
#endif
}
#endif

content::WebContents* ChromeViewsDelegate::CreateWebContents(
    content::BrowserContext* browser_context,
    content::SiteInstance* site_instance) {
  return NULL;
}
