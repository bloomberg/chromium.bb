// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/taskbar_icon_finder.h"

#include <windows.h>

#include <objbase.h>
#include <oleauto.h>
#include <uiautomation.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"

namespace {

// TaskbarIconFinder -----------------------------------------------------------

// A class that uses UIAutomation in a COM multi-threaded apartment thread to
// find the bounding rectangle of Chrome's taskbar icon on the system's primary
// monitor.
class TaskbarIconFinder {
 public:
  // The result of a search for Chrome's taskbar icon. An empty rect is provided
  // case of error or if no icon can be found.
  using ResultCallback = base::OnceCallback<void(const gfx::Rect&)>;

  // Finds the bounding rectangle of Chrome's taskbar icon on the primary
  // monitor, running |result_callback| with the result when done.
  static void FindAsync(ResultCallback result_callback);

 private:
  // Constructs a new finder and immediately starts running it on a dedicated
  // automation thread in a multi-threaded COM apartment.
  explicit TaskbarIconFinder(ResultCallback result_callback);

  // Receives the result computed on the automation thread, stops the automation
  // thread, passes the results to the caller, then self-destructs.
  void OnComplete(const gfx::Rect& rect);

  // Main function for the finder's automation thread. Bounces the results of
  // the operation back to OnComplete on the caller's sequenced task runner
  // (|finder_runner|).
  static void RunOnComThread(
      scoped_refptr<base::SequencedTaskRunner> finder_runner,
      TaskbarIconFinder* finder);

  // Returns the values of the |property_id| property (of type VT_R8 | VT_ARRAY)
  // cached in |element|. May only be used on the automation thread.
  static std::vector<double> GetCachedDoubleArrayValue(
      IUIAutomationElement* element,
      PROPERTYID property_id);

  // Populates |rect| with the bounding rectangle of any item in |icons| that is
  // on the primary monitor. |rect| is unmodified if no such item/rect is found.
  // May only be used on the automation thread.
  static void FindRectOnPrimaryMonitor(IUIAutomation* automation,
                                       IUIAutomationElementArray* icons,
                                       gfx::Rect* rect);

  // Finds an item with an automation id matching Chrome's app user model id.
  // Returns the first failure HRESULT, or the final success HRESULT. On
  // success, |rect| is populated with the bouning rectangle of the icon if
  // found.
  static HRESULT DoOnComThread(gfx::Rect* rect);

  // A thread in the COM MTA in which automation calls are made.
  base::Thread automation_thread_;

  // The caller's callback.
  ResultCallback result_callback_;

  DISALLOW_COPY_AND_ASSIGN(TaskbarIconFinder);
};

// static
void TaskbarIconFinder::FindAsync(ResultCallback result_callback) {
  DCHECK(result_callback);
  // The instance self-destructs in OnComplete.
  new TaskbarIconFinder(std::move(result_callback));
}

TaskbarIconFinder::TaskbarIconFinder(ResultCallback result_callback)
    : automation_thread_("TaskbarIconFinder"),
      result_callback_(std::move(result_callback)) {
  DCHECK(result_callback_);
  // Start the automation thread and run the finder on it.
  automation_thread_.init_com_with_mta(true);
  automation_thread_.Start();
  automation_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&TaskbarIconFinder::RunOnComThread,
                            base::SequencedTaskRunnerHandle::Get(),
                            base::Unretained(this)));
}

void TaskbarIconFinder::OnComplete(const gfx::Rect& rect) {
  automation_thread_.Stop();
  std::move(result_callback_).Run(rect);
  delete this;
}

// static
void TaskbarIconFinder::RunOnComThread(
    scoped_refptr<base::SequencedTaskRunner> finder_runner,
    TaskbarIconFinder* finder) {
  // This and all methods below must be called on the automation thread.
  DCHECK(!finder_runner->RunsTasksOnCurrentThread());

  gfx::Rect rect;
  DoOnComThread(&rect);
  finder_runner->PostTask(FROM_HERE,
                          base::Bind(&TaskbarIconFinder::OnComplete,
                                     base::Unretained(finder), rect));
}

// static
std::vector<double> TaskbarIconFinder::GetCachedDoubleArrayValue(
    IUIAutomationElement* element,
    PROPERTYID property_id) {
  std::vector<double> values;
  base::win::ScopedVariant var;

  if (FAILED(element->GetCachedPropertyValueEx(property_id, TRUE,
                                               var.Receive()))) {
    return values;
  }

  if (V_VT(var.ptr()) != (VT_R8 | VT_ARRAY)) {
    LOG_IF(ERROR, V_VT(var.ptr()) != VT_UNKNOWN)
        << __func__ << " property is not an R8 array: " << V_VT(var.ptr());
    return values;
  }

  SAFEARRAY* array = V_ARRAY(var.ptr());
  if (SafeArrayGetDim(array) != 1)
    return values;
  long lower_bound = 0;
  long upper_bound = 0;
  SafeArrayGetLBound(array, 1, &lower_bound);
  SafeArrayGetUBound(array, 1, &upper_bound);
  if (lower_bound || upper_bound <= lower_bound)
    return values;
  double* data = nullptr;
  SafeArrayAccessData(array, reinterpret_cast<void**>(&data));
  values.assign(data, data + upper_bound + 1);
  SafeArrayUnaccessData(array);

  return values;
}

// static
void TaskbarIconFinder::FindRectOnPrimaryMonitor(
    IUIAutomation* automation,
    IUIAutomationElementArray* icons,
    gfx::Rect* rect) {
  int length = 0;
  icons->get_Length(&length);

  // Find each icon's nearest ancestor with an HWND.
  base::win::ScopedComPtr<IUIAutomationTreeWalker> tree_walker;
  HRESULT result = automation->get_RawViewWalker(tree_walker.GetAddressOf());
  if (FAILED(result) || !tree_walker)
    return;
  base::win::ScopedComPtr<IUIAutomationCacheRequest> cache_request;
  result = automation->CreateCacheRequest(cache_request.GetAddressOf());
  if (FAILED(result) || !cache_request)
    return;
  cache_request->AddProperty(UIA_NativeWindowHandlePropertyId);

  base::win::ScopedComPtr<IUIAutomationElement> icon;
  for (int i = 0; i < length; ++i) {
    HWND hwnd = 0;
    icons->GetElement(i, icon.GetAddressOf());

    // Walk up the tree to find the icon's first parent with an HWND.
    base::win::ScopedComPtr<IUIAutomationElement> search = icon;
    while (true) {
      base::win::ScopedComPtr<IUIAutomationElement> parent;
      result = tree_walker->GetParentElementBuildCache(
          search.Get(), cache_request.Get(), parent.GetAddressOf());
      if (FAILED(result) || !parent)
        break;
      base::win::ScopedVariant var;
      result = parent->GetCachedPropertyValueEx(
          UIA_NativeWindowHandlePropertyId, TRUE, var.Receive());
      if (FAILED(result))
        break;
      hwnd = reinterpret_cast<HWND>(V_I4(var.ptr()));
      if (hwnd)
        break;  // Found.
      search.Reset();
      std::swap(parent, search);
    }

    // No parent hwnd found for this icon.
    if (!hwnd)
      continue;

    // Is this icon's window on the primary monitor?
    HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (monitor && ::GetMonitorInfo(monitor, &monitor_info) &&
        (monitor_info.dwFlags & MONITORINFOF_PRIMARY) != 0) {
      break;  // All done.
    }
    icon.Reset();
  }

  if (!icon)
    return;  // No taskbar icon found on the primary monitor.

  std::vector<double> bounding_rect =
      GetCachedDoubleArrayValue(icon.Get(), UIA_BoundingRectanglePropertyId);
  if (!bounding_rect.empty()) {
    *rect = gfx::Rect(bounding_rect[0], bounding_rect[1], bounding_rect[2],
                      bounding_rect[3]);
  }
}

// static
HRESULT TaskbarIconFinder::DoOnComThread(gfx::Rect* rect) {
  base::win::ScopedComPtr<IUIAutomation> automation;
  HRESULT result =
      ::CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&automation));
  if (FAILED(result) || !automation)
    return result;

  // Create a condition: automation_id=ap_user_model_id && type=button_type.
  base::win::ScopedVariant app_user_model_id(
      ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()).c_str());
  base::win::ScopedComPtr<IUIAutomationCondition> id_condition;
  result = automation->CreatePropertyCondition(UIA_AutomationIdPropertyId,
                                               app_user_model_id,
                                               id_condition.GetAddressOf());
  if (FAILED(result) || !id_condition)
    return result;

  base::win::ScopedVariant button_type(UIA_ButtonControlTypeId);
  base::win::ScopedComPtr<IUIAutomationCondition> type_condition;
  result = automation->CreatePropertyCondition(
      UIA_ControlTypePropertyId, button_type, type_condition.GetAddressOf());
  if (FAILED(result) || !type_condition)
    return result;

  base::win::ScopedComPtr<IUIAutomationCondition> condition;
  result = automation->CreateAndCondition(
      id_condition.Get(), type_condition.Get(), condition.GetAddressOf());

  // Cache the bounding rectangle of all found items.
  base::win::ScopedComPtr<IUIAutomationCacheRequest> cache_request;
  result = automation->CreateCacheRequest(cache_request.GetAddressOf());
  if (FAILED(result) || !cache_request)
    return result;
  cache_request->AddProperty(UIA_BoundingRectanglePropertyId);

  // Search the desktop to find all buttons with the correct automation id.
  base::win::ScopedComPtr<IUIAutomationElement> desktop;
  result = automation->GetRootElement(desktop.GetAddressOf());
  if (FAILED(result) || !desktop)
    return result;

  base::win::ScopedComPtr<IUIAutomationElementArray> icons;
  result =
      desktop->FindAllBuildCache(TreeScope_Subtree, condition.Get(),
                                 cache_request.Get(), icons.GetAddressOf());
  if (FAILED(result) || !icons)
    return result;

  // Pick the icon on the primary monitor.
  FindRectOnPrimaryMonitor(automation.Get(), icons.Get(), rect);
  return S_OK;
}

// Utilities -------------------------------------------------------------------

// Sets |result_storage| with the value provided through invocation of the
// callback in |result|, then runs |quit_closure|.
void SetResultAndContinue(base::Closure quit_closure,
                          gfx::Rect* result_storage,
                          const gfx::Rect& result) {
  *result_storage = result;
  quit_closure.Run();
}

}  // namespace

gfx::Rect FindTaskbarIconModal() {
  gfx::Rect result;

  base::RunLoop run_loop;
  TaskbarIconFinder::FindAsync(base::BindOnce(&SetResultAndContinue,
                                              run_loop.QuitClosure(),
                                              base::Unretained(&result)));
  run_loop.Run();

  return result;
}
