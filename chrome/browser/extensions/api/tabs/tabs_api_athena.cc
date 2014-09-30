// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"

using content::WebContents;

namespace extensions {

namespace windows = api::windows;
namespace keys = tabs_constants;
namespace tabs = api::tabs;

void ZoomModeToZoomSettings(ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings) {
  DCHECK(zoom_settings);
  NOTIMPLEMENTED();
}

// Windows ---------------------------------------------------------------------

bool WindowsGetFunction::RunSync() {
  scoped_ptr<windows::Get::Params> params(windows::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool WindowsGetCurrentFunction::RunSync() {
  scoped_ptr<windows::GetCurrent::Params> params(
      windows::GetCurrent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool WindowsGetLastFocusedFunction::RunSync() {
  scoped_ptr<windows::GetLastFocused::Params> params(
      windows::GetLastFocused::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool WindowsGetAllFunction::RunSync() {
  scoped_ptr<windows::GetAll::Params> params(
      windows::GetAll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool WindowsCreateFunction::ShouldOpenIncognitoWindow(
    const windows::Create::Params::CreateData* create_data,
    std::vector<GURL>* urls, bool* is_error) {
  error_ = keys::kIncognitoModeIsDisabled;
  *is_error = true;
  NOTIMPLEMENTED();
  return false;
}

bool WindowsCreateFunction::RunSync() {
  scoped_ptr<windows::Create::Params> params(
      windows::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  NOTIMPLEMENTED();
  return false;
}

bool WindowsUpdateFunction::RunSync() {
  scoped_ptr<windows::Update::Params> params(
      windows::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  NOTIMPLEMENTED();
  return false;
}

bool WindowsRemoveFunction::RunSync() {
  scoped_ptr<windows::Remove::Params> params(
      windows::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  NOTIMPLEMENTED();
  return false;
}

// Tabs ------------------------------------------------------------------------

bool TabsGetSelectedFunction::RunSync() {
  scoped_ptr<windows::Remove::Params> params(
      windows::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool TabsGetAllInWindowFunction::RunSync() {
  scoped_ptr<tabs::GetAllInWindow::Params> params(
      tabs::GetAllInWindow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool TabsQueryFunction::RunSync() {
  scoped_ptr<tabs::Query::Params> params(tabs::Query::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool TabsCreateFunction::RunSync() {
  scoped_ptr<tabs::Create::Params> params(tabs::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool TabsDuplicateFunction::RunSync() {
  scoped_ptr<tabs::Duplicate::Params> params(
      tabs::Duplicate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool TabsGetFunction::RunSync() {
  scoped_ptr<tabs::Get::Params> params(tabs::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool TabsGetCurrentFunction::RunSync() {
  DCHECK(dispatcher());
  NOTIMPLEMENTED();
  return false;
}

bool TabsHighlightFunction::RunSync() {
  scoped_ptr<tabs::Highlight::Params> params(
      tabs::Highlight::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool TabsHighlightFunction::HighlightTab(TabStripModel* tabstrip,
                                         ui::ListSelectionModel* selection,
                                         int* active_index,
                                         int index) {
  NOTREACHED();
  return false;
}

TabsUpdateFunction::TabsUpdateFunction() : web_contents_(NULL) {
}

bool TabsUpdateFunction::RunAsync() {
  scoped_ptr<tabs::Update::Params> params(tabs::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool TabsUpdateFunction::UpdateURL(const std::string &url_string,
                                   int tab_id,
                                   bool* is_async) {
  NOTREACHED();
  return false;
}

void TabsUpdateFunction::PopulateResult() {
  NOTREACHED();
}

void TabsUpdateFunction::OnExecuteCodeFinished(
    const std::string& error,
    const GURL& url,
    const base::ListValue& script_result) {
  NOTREACHED();
}

bool TabsMoveFunction::RunSync() {
  scoped_ptr<tabs::Move::Params> params(tabs::Move::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool TabsMoveFunction::MoveTab(int tab_id,
                               int* new_index,
                               int iteration,
                               base::ListValue* tab_values,
                               int* window_id) {
  NOTREACHED();
  return false;
}

bool TabsReloadFunction::RunSync() {
  scoped_ptr<tabs::Reload::Params> params(
      tabs::Reload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool TabsRemoveFunction::RunSync() {
  scoped_ptr<tabs::Remove::Params> params(tabs::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

bool TabsRemoveFunction::RemoveTab(int tab_id) {
  NOTREACHED();
  return false;
}

TabsCaptureVisibleTabFunction::TabsCaptureVisibleTabFunction()
    : chrome_details_(this) {
}

bool TabsCaptureVisibleTabFunction::IsScreenshotEnabled() {
  NOTREACHED();
  return false;
}

WebContents* TabsCaptureVisibleTabFunction::GetWebContentsForID(int window_id) {
  NOTREACHED();
  return NULL;
}

void TabsCaptureVisibleTabFunction::OnCaptureFailure(FailureReason reason) {
  NOTREACHED();
}

void TabsCaptureVisibleTabFunction::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  NOTIMPLEMENTED();
}

bool TabsDetectLanguageFunction::RunAsync() {
  scoped_ptr<tabs::DetectLanguage::Params> params(
      tabs::DetectLanguage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return false;
}

void TabsDetectLanguageFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  NOTREACHED();
}

void TabsDetectLanguageFunction::GotLanguage(const std::string& language) {
  NOTREACHED();
}

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction()
    : chrome_details_(this), execute_tab_id_(-1) {
  (void) execute_tab_id_;
}

ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() {}

bool ExecuteCodeInTabFunction::HasPermission() {
  NOTREACHED();
  return false;
}

bool ExecuteCodeInTabFunction::Init() {
  NOTREACHED();
  return false;
}

bool ExecuteCodeInTabFunction::CanExecuteScriptOnPage() {
  NOTREACHED();
  return false;
}

ScriptExecutor* ExecuteCodeInTabFunction::GetScriptExecutor() {
  NOTREACHED();
  return NULL;
}

bool ExecuteCodeInTabFunction::IsWebView() const {
  NOTREACHED();
  return false;
}

const GURL& ExecuteCodeInTabFunction::GetWebViewSrc() const {
  NOTREACHED();
  return GURL::EmptyGURL();
}

bool TabsExecuteScriptFunction::ShouldInsertCSS() const {
  NOTREACHED();
  return false;
}

void TabsExecuteScriptFunction::OnExecuteCodeFinished(
    const std::string& error,
    const GURL& on_url,
    const base::ListValue& result) {
  NOTREACHED();
}

bool TabsInsertCSSFunction::ShouldInsertCSS() const {
  NOTREACHED();
  return true;
}

content::WebContents* ZoomAPIFunction::GetWebContents(int tab_id) {
  NOTREACHED();
  return NULL;
}

bool TabsSetZoomFunction::RunAsync() {
  scoped_ptr<tabs::SetZoom::Params> params(
      tabs::SetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  NOTIMPLEMENTED();
  return false;
}

bool TabsGetZoomFunction::RunAsync() {
  scoped_ptr<tabs::GetZoom::Params> params(
      tabs::GetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  NOTIMPLEMENTED();
  return false;
}

bool TabsSetZoomSettingsFunction::RunAsync() {
  using api::tabs::ZoomSettings;
  scoped_ptr<tabs::SetZoomSettings::Params> params(
      tabs::SetZoomSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  NOTIMPLEMENTED();
  return false;
}

bool TabsGetZoomSettingsFunction::RunAsync() {
  scoped_ptr<tabs::GetZoomSettings::Params> params(
      tabs::GetZoomSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  NOTIMPLEMENTED();
  return false;
}

}  // namespace extensions
