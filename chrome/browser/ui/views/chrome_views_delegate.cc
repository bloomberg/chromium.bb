// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include <memory>

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/keep_alive_types.h"
#include "chrome/browser/lifetime/scoped_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/views/harmony/layout_delegate.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_factory.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#endif

// Helpers --------------------------------------------------------------------

namespace {

Profile* GetProfileForWindow(const views::Widget* window) {
  if (!window)
    return NULL;
  return reinterpret_cast<Profile*>(
      window->GetNativeWindowProperty(Profile::kProfileKey));
}

// If the given window has a profile associated with it, use that profile's
// preference service. Otherwise, store and retrieve the data from Local State.
// This function may return NULL if the necessary pref service has not yet
// been initialized.
// TODO(mirandac): This function will also separate windows by profile in a
// multi-profile environment.
PrefService* GetPrefsForWindow(const views::Widget* window) {
  Profile* profile = GetProfileForWindow(window);
  if (!profile) {
    // Use local state for windows that have no explicit profile.
    return g_browser_process->local_state();
  }
  return profile->GetPrefs();
}

}  // namespace


// ChromeViewsDelegate --------------------------------------------------------

#if !defined(OS_WIN)
ChromeViewsDelegate::ChromeViewsDelegate() {}
#endif

ChromeViewsDelegate::~ChromeViewsDelegate() {
  DCHECK_EQ(0u, ref_count_);
}

void ChromeViewsDelegate::SaveWindowPlacement(const views::Widget* window,
                                              const std::string& window_name,
                                              const gfx::Rect& bounds,
                                              ui::WindowShowState show_state) {
  PrefService* prefs = GetPrefsForWindow(window);
  if (!prefs)
    return;

  std::unique_ptr<DictionaryPrefUpdate> pref_update =
      chrome::GetWindowPlacementDictionaryReadWrite(window_name, prefs);
  base::DictionaryValue* window_preferences = pref_update->Get();
  window_preferences->SetInteger("left", bounds.x());
  window_preferences->SetInteger("top", bounds.y());
  window_preferences->SetInteger("right", bounds.right());
  window_preferences->SetInteger("bottom", bounds.bottom());
  window_preferences->SetBoolean("maximized",
                                 show_state == ui::SHOW_STATE_MAXIMIZED);
  // TODO(afakhry): Remove Docked Windows in M58.
  window_preferences->SetBoolean("docked", show_state == ui::SHOW_STATE_DOCKED);

  gfx::Rect work_area(display::Screen::GetScreen()
                          ->GetDisplayNearestWindow(window->GetNativeView())
                          .work_area());
  window_preferences->SetInteger("work_area_left", work_area.x());
  window_preferences->SetInteger("work_area_top", work_area.y());
  window_preferences->SetInteger("work_area_right", work_area.right());
  window_preferences->SetInteger("work_area_bottom", work_area.bottom());
}

bool ChromeViewsDelegate::GetSavedWindowPlacement(
    const views::Widget* widget,
    const std::string& window_name,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  PrefService* prefs = g_browser_process->local_state();
  if (!prefs)
    return false;

  DCHECK(prefs->FindPreference(window_name));
  const base::DictionaryValue* dictionary = prefs->GetDictionary(window_name);
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
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

#if defined(OS_CHROMEOS)
  AdjustSavedWindowPlacementChromeOS(widget, bounds);
#endif
  return true;
}

void ChromeViewsDelegate::NotifyAccessibilityEvent(
    views::View* view, ui::AXEvent event_type) {
#if defined(USE_AURA)
  AutomationManagerAura::GetInstance()->HandleEvent(
      GetProfileForWindow(view->GetWidget()), view, event_type);
#endif
}

void ChromeViewsDelegate::AddRef() {
  if (ref_count_ == 0u) {
    keep_alive_.reset(
        new ScopedKeepAlive(KeepAliveOrigin::CHROME_VIEWS_DELEGATE,
                            KeepAliveRestartOption::DISABLED));
  }

  ++ref_count_;
}

void ChromeViewsDelegate::ReleaseRef() {
  DCHECK_NE(0u, ref_count_);

  if (--ref_count_ == 0u)
    keep_alive_.reset();
}

void ChromeViewsDelegate::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  // We need to determine opacity if it's not already specified.
  if (params->opacity == views::Widget::InitParams::INFER_OPACITY)
    params->opacity = GetOpacityForInitParams(*params);

  // If we already have a native_widget, we don't have to try to come
  // up with one.
  if (params->native_widget)
    return;

  if (!native_widget_factory().is_null()) {
    params->native_widget = native_widget_factory().Run(*params, delegate);
    if (params->native_widget)
      return;
  }

  params->native_widget = CreateNativeWidget(params, delegate);
}

ui::ContextFactory* ChromeViewsDelegate::GetContextFactory() {
  return content::GetContextFactory();
}

ui::ContextFactoryPrivate* ChromeViewsDelegate::GetContextFactoryPrivate() {
  return content::GetContextFactoryPrivate();
}

std::string ChromeViewsDelegate::GetApplicationName() {
  return version_info::GetProductName();
}

scoped_refptr<base::TaskRunner>
ChromeViewsDelegate::GetBlockingPoolTaskRunner() {
  return content::BrowserThread::GetBlockingPool();
}

gfx::Insets ChromeViewsDelegate::GetDialogButtonInsets() const {
  const LayoutDelegate* layout_delegate = LayoutDelegate::Get();
  const int top = layout_delegate->GetMetric(
      LayoutDelegate::Metric::DIALOG_BUTTON_TOP_SPACING);
  const int margin = layout_delegate->GetMetric(
      LayoutDelegate::Metric::DIALOG_BUTTON_MARGIN);
  return gfx::Insets(top, margin, margin, margin);
}

int ChromeViewsDelegate::GetDialogCloseButtonMargin() const {
  return LayoutDelegate::Get()->GetMetric(
      LayoutDelegate::Metric::DIALOG_CLOSE_BUTTON_MARGIN);
}

int ChromeViewsDelegate::GetDialogRelatedButtonHorizontalSpacing() const {
  return LayoutDelegate::Get()->GetMetric(
      LayoutDelegate::Metric::RELATED_BUTTON_HORIZONTAL_SPACING);
}

int ChromeViewsDelegate::GetDialogRelatedControlVerticalSpacing() const {
  return LayoutDelegate::Get()->GetMetric(
      LayoutDelegate::Metric::RELATED_CONTROL_VERTICAL_SPACING);
}

gfx::Insets ChromeViewsDelegate::GetDialogFrameViewInsets() const {
  const LayoutDelegate* layout_delegate = LayoutDelegate::Get();
  const int top = layout_delegate->GetMetric(
      LayoutDelegate::Metric::PANEL_CONTENT_MARGIN);
  const int side = layout_delegate->GetMetric(
      LayoutDelegate::Metric::DIALOG_BUTTON_MARGIN);
  // Titles are inset at the top and sides, but not at the bottom.
  return gfx::Insets(top, side, 0, side);
}

gfx::Insets ChromeViewsDelegate::GetBubbleDialogMargins() const {
  return gfx::Insets(LayoutDelegate::Get()->GetMetric(
      LayoutDelegate::Metric::PANEL_CONTENT_MARGIN));
}

int ChromeViewsDelegate::GetButtonMinimumWidth() const {
  return LayoutDelegate::Get()->GetMetric(
      LayoutDelegate::Metric::BUTTON_MINIMUM_WIDTH);
}

int ChromeViewsDelegate::GetDialogButtonMinimumWidth() const {
  return LayoutDelegate::Get()->GetMetric(
      LayoutDelegate::Metric::DIALOG_BUTTON_MINIMUM_WIDTH);
}

int ChromeViewsDelegate::GetButtonHorizontalPadding() const {
  return LayoutDelegate::Get()->GetMetric(
      LayoutDelegate::Metric::BUTTON_HORIZONTAL_PADDING);
}

#if !defined(OS_CHROMEOS)
views::Widget::InitParams::WindowOpacity
ChromeViewsDelegate::GetOpacityForInitParams(
    const views::Widget::InitParams& params) {
  return views::Widget::InitParams::OPAQUE_WINDOW;
}
#endif
