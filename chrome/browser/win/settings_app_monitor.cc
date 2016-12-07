// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/settings_app_monitor.h"

#include <atlbase.h>
#include <atlcom.h>
#include <oleauto.h>
#include <stdint.h>
#include <uiautomation.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/pattern.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "ui/base/win/atl_module.h"

namespace shell_integration {
namespace win {

// Each item represent one UI element in the Settings App.
enum class ElementType {
  // The "Web browser" element in the "Default apps" pane.
  DEFAULT_BROWSER,
  // The element representing a browser in the "Choose an app" popup.
  BROWSER_BUTTON,
  // The button labeled "Check it out" that leaves Edge as the default browser.
  CHECK_IT_OUT,
  // The button labeled "Switch Anyway" that dismisses the Edge promo.
  SWITCH_ANYWAY,
  // Any other element.
  UNKNOWN,
};


// SettingsAppMonitor::Context -------------------------------------------------

// The guts of the monitor which runs on a dedicated thread in the
// multi-threaded COM apartment. An instance may be constructed on any thread,
// but Initialize() must be invoked on a thread in the MTA.
class SettingsAppMonitor::Context {
 public:
  // Returns a new instance ready for initialization and use on another thread.
  static base::WeakPtr<Context> Create();

  // Deletes the instance.
  void DeleteOnAutomationThread();

  // Initializes the context, invoking the monitor's |OnInitialized| method via
  // |monitor_runner| when done. On success, the monitor's other On* methods
  // will be invoked as events are observed. On failure, this instance
  // self-destructs after posting |init_callback|. |task_runner| is the runner
  // for the dedicated thread on which the context lives (owned by its
  // UIAutomationClient).
  void Initialize(base::SingleThreadTaskRunner* task_runner,
                  scoped_refptr<base::SequencedTaskRunner> monitor_runner,
                  const base::WeakPtr<SettingsAppMonitor>& monitor);

 private:
  class EventHandler;

  // The one and only method that may be called from outside of the automation
  // thread.
  Context();
  ~Context();

  // Method(s) invoked by event handlers via weak pointers.

  // Handles a focus change event on |sender|. Dispatches OnAppFocused if
  // |sender| is the settings app.
  void HandleFocusChangedEvent(
      base::win::ScopedComPtr<IUIAutomationElement> sender);

  // Handles the invocation of the element that opens the browser chooser.
  void HandleChooserInvoked();

  // Handles the invocation of an element in the browser chooser.
  void HandleBrowserChosen(const base::string16& browser_name);

  // Handles the invocation of an element in the Edge promo.
  void HandlePromoChoiceMade(bool accept_promo);

  // Returns an event handler for all event types of interest.
  base::win::ScopedComPtr<IUnknown> GetEventHandler();

  // Returns a pointer to the event handler's generic interface.
  base::win::ScopedComPtr<IUIAutomationEventHandler>
  GetAutomationEventHandler();

  // Returns a pointer to the event handler's focus changed interface.
  base::win::ScopedComPtr<IUIAutomationFocusChangedEventHandler>
  GetFocusChangedEventHandler();

  // Installs an event handler to observe events of interest.
  HRESULT InstallObservers();

  // The task runner for the automation thread.
  base::SingleThreadTaskRunner* task_runner_ = nullptr;

  // The task runner on which the owning monitor lives.
  scoped_refptr<base::SequencedTaskRunner> monitor_runner_ = nullptr;

  // The monitor that owns this context.
  base::WeakPtr<SettingsAppMonitor> monitor_;

  // The automation client.
  base::win::ScopedComPtr<IUIAutomation> automation_;

  // The event handler.
  base::win::ScopedComPtr<IUnknown> event_handler_;

  // State to suppress duplicate "focus changed" events.
  ElementType last_focused_element_ = ElementType::UNKNOWN;

  // Weak pointers to the context are given to event handlers.
  base::WeakPtrFactory<Context> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};


// SettingsAppMonitor::Context::EventHandler -----------------------------------

// A handler of events on arbitrary threads in the MTA.
class SettingsAppMonitor::Context::EventHandler
    : public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>,
      public IUIAutomationEventHandler,
      public IUIAutomationFocusChangedEventHandler {
 public:
  BEGIN_COM_MAP(SettingsAppMonitor::Context::EventHandler)
    COM_INTERFACE_ENTRY(IUIAutomationEventHandler)
    COM_INTERFACE_ENTRY(IUIAutomationFocusChangedEventHandler)
  END_COM_MAP()

  EventHandler();
  ~EventHandler();

  // Initializes the object. Events will be dispatched back to |context| via
  // |context_runner|.
  void Initialize(scoped_refptr<base::SingleThreadTaskRunner> context_runner,
                  const base::WeakPtr<SettingsAppMonitor::Context>& context,
                  base::win::ScopedComPtr<IUIAutomation> automation);

  // IUIAutomationEventHandler:
  STDMETHOD(HandleAutomationEvent)(IUIAutomationElement *sender,
                                   EVENTID eventId) override;

  // IUIAutomationFocusChangedEventHandler:
  STDMETHOD(HandleFocusChangedEvent)(IUIAutomationElement* sender) override;

 private:
  // The task runner for the monitor client context.
  scoped_refptr<base::SingleThreadTaskRunner> context_runner_;

  // The monitor context that owns this event handler.
  base::WeakPtr<SettingsAppMonitor::Context> context_;

  base::win::ScopedComPtr<IUIAutomation> automation_;

  DISALLOW_COPY_AND_ASSIGN(EventHandler);
};


// Utility functions -----------------------------------------------------------

base::string16 GetCachedBstrValue(IUIAutomationElement* element,
                                  PROPERTYID property_id) {
  HRESULT result = S_OK;
  base::win::ScopedVariant var;

  result = element->GetCachedPropertyValueEx(property_id, TRUE, var.Receive());
  if (FAILED(result))
    return base::string16();

  if (V_VT(var.ptr()) != VT_BSTR) {
    LOG_IF(ERROR, V_VT(var.ptr()) != VT_UNKNOWN)
        << __func__ << " property is not a BSTR: " << V_VT(var.ptr());
    return base::string16();
  }

  return base::string16(V_BSTR(var.ptr()));
}

// Configures a cache request so that it includes all properties needed by
// DetectElementType() to detect the elements of interest.
void ConfigureCacheRequest(IUIAutomationCacheRequest* cache_request) {
  DCHECK(cache_request);
  cache_request->AddProperty(UIA_AutomationIdPropertyId);
  cache_request->AddProperty(UIA_NamePropertyId);
  cache_request->AddProperty(UIA_ClassNamePropertyId);
#if DCHECK_IS_ON()
  cache_request->AddProperty(UIA_ControlTypePropertyId);
  cache_request->AddProperty(UIA_IsPeripheralPropertyId);
  cache_request->AddProperty(UIA_ProcessIdPropertyId);
  cache_request->AddProperty(UIA_ValueValuePropertyId);
  cache_request->AddProperty(UIA_RuntimeIdPropertyId);
#endif  // DCHECK_IS_ON()
}

// Helper function to get the parent element with class name "Flyout". Used to
// determine the |element|'s type.
base::string16 GetFlyoutParentAutomationId(IUIAutomation* automation,
                                           IUIAutomationElement* element) {
  // Create a condition that will include only elements with the right class
  // name in the tree view.
  base::win::ScopedVariant class_name(L"Flyout");
  base::win::ScopedComPtr<IUIAutomationCondition> condition;
  HRESULT result = automation->CreatePropertyCondition(
      UIA_ClassNamePropertyId, class_name, condition.Receive());
  if (FAILED(result))
    return base::string16();

  base::win::ScopedComPtr<IUIAutomationTreeWalker> tree_walker;
  result = automation->CreateTreeWalker(condition.get(), tree_walker.Receive());
  if (FAILED(result))
    return base::string16();

  base::win::ScopedComPtr<IUIAutomationCacheRequest> cache_request;
  result = automation->CreateCacheRequest(cache_request.Receive());
  if (FAILED(result))
    return base::string16();
  ConfigureCacheRequest(cache_request.get());

  // From MSDN, NormalizeElementBuildCache() "Retrieves the ancestor element
  // nearest to the specified Microsoft UI Automation element in the tree view".
  IUIAutomationElement* flyout_element = nullptr;
  result = tree_walker->NormalizeElementBuildCache(element, cache_request.get(),
                                                   &flyout_element);
  if (FAILED(result) || !flyout_element)
    return base::string16();

  return GetCachedBstrValue(flyout_element, UIA_AutomationIdPropertyId);
}

ElementType DetectElementType(IUIAutomation* automation,
                              IUIAutomationElement* sender) {
  DCHECK(automation);
  DCHECK(sender);
  base::string16 aid(GetCachedBstrValue(sender, UIA_AutomationIdPropertyId));
  if (aid == L"SystemSettings_DefaultApps_Browser_Button")
    return ElementType::DEFAULT_BROWSER;
  if (aid == L"SystemSettings_DefaultApps_Browser_App0_HyperlinkButton")
    return ElementType::SWITCH_ANYWAY;
  if (base::MatchPattern(aid, L"SystemSettings_DefaultApps_Browser_*_Button")) {
    // This element type depends on the automation id of one of its ancestors.
    base::string16 automation_id =
        GetFlyoutParentAutomationId(automation, sender);
    if (automation_id == L"settingsFlyout")
      return ElementType::CHECK_IT_OUT;
    else if (automation_id == L"DefaultAppsFlyoutPresenter")
      return ElementType::BROWSER_BUTTON;
  }
  return ElementType::UNKNOWN;
}


// Debug logging utility functions ---------------------------------------------

bool GetCachedBoolValue(IUIAutomationElement* element, PROPERTYID property_id) {
#if DCHECK_IS_ON()
  base::win::ScopedVariant var;

  if (FAILED(element->GetCachedPropertyValueEx(property_id, TRUE,
                                               var.Receive()))) {
    return false;
  }

  if (V_VT(var.ptr()) != VT_BOOL) {
    LOG_IF(ERROR, V_VT(var.ptr()) != VT_UNKNOWN)
        << __func__ << " property is not a BOOL: " << V_VT(var.ptr());
    return false;
  }

  return V_BOOL(var.ptr()) != 0;
#else   // DCHECK_IS_ON()
  return false;
#endif  // !DCHECK_IS_ON()
}

int32_t GetCachedInt32Value(IUIAutomationElement* element,
                            PROPERTYID property_id) {
#if DCHECK_IS_ON()
  base::win::ScopedVariant var;

  if (FAILED(element->GetCachedPropertyValueEx(property_id, TRUE,
                                               var.Receive()))) {
    return false;
  }

  if (V_VT(var.ptr()) != VT_I4) {
    LOG_IF(ERROR, V_VT(var.ptr()) != VT_UNKNOWN)
        << __func__ << " property is not an I4: " << V_VT(var.ptr());
    return false;
  }

  return V_I4(var.ptr());
#else   // DCHECK_IS_ON()
  return 0;
#endif  // !DCHECK_IS_ON()
}

std::vector<int32_t> GetCachedInt32ArrayValue(IUIAutomationElement* element,
                                              PROPERTYID property_id) {
  std::vector<int32_t> values;
#if DCHECK_IS_ON()
  base::win::ScopedVariant var;

  if (FAILED(element->GetCachedPropertyValueEx(property_id, TRUE,
                                               var.Receive()))) {
    return values;
  }

  if (V_VT(var.ptr()) != (VT_I4 | VT_ARRAY)) {
    LOG_IF(ERROR, V_VT(var.ptr()) != VT_UNKNOWN)
        << __func__ << " property is not an I4 array: " << V_VT(var.ptr());
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
  int32_t* data = nullptr;
  SafeArrayAccessData(array, reinterpret_cast<void**>(&data));
  values.assign(data, data + upper_bound + 1);
  SafeArrayUnaccessData(array);
#endif  // DCHECK_IS_ON()
  return values;
}

std::string IntArrayToString(const std::vector<int32_t>& values) {
#if DCHECK_IS_ON()
  std::vector<std::string> value_strings;
  std::transform(values.begin(), values.end(),
                 std::back_inserter(value_strings), &base::IntToString);
  return base::JoinString(value_strings, ", ");
#else   // DCHECK_IS_ON()
  return std::string();
#endif  // !DCHECK_IS_ON()
}

const char* GetEventName(EVENTID event_id) {
#if DCHECK_IS_ON()
  switch (event_id) {
    case UIA_ToolTipOpenedEventId:
      return "UIA_ToolTipOpenedEventId";
    case UIA_ToolTipClosedEventId:
      return "UIA_ToolTipClosedEventId";
    case UIA_StructureChangedEventId:
      return "UIA_StructureChangedEventId";
    case UIA_MenuOpenedEventId:
      return "UIA_MenuOpenedEventId";
    case UIA_AutomationPropertyChangedEventId:
      return "UIA_AutomationPropertyChangedEventId";
    case UIA_AutomationFocusChangedEventId:
      return "UIA_AutomationFocusChangedEventId";
    case UIA_AsyncContentLoadedEventId:
      return "UIA_AsyncContentLoadedEventId";
    case UIA_MenuClosedEventId:
      return "UIA_MenuClosedEventId";
    case UIA_LayoutInvalidatedEventId:
      return "UIA_LayoutInvalidatedEventId";
    case UIA_Invoke_InvokedEventId:
      return "UIA_Invoke_InvokedEventId";
    case UIA_SelectionItem_ElementAddedToSelectionEventId:
      return "UIA_SelectionItem_ElementAddedToSelectionEventId";
    case UIA_SelectionItem_ElementRemovedFromSelectionEventId:
      return "UIA_SelectionItem_ElementRemovedFromSelectionEventId";
    case UIA_SelectionItem_ElementSelectedEventId:
      return "UIA_SelectionItem_ElementSelectedEventId";
    case UIA_Selection_InvalidatedEventId:
      return "UIA_Selection_InvalidatedEventId";
    case UIA_Text_TextSelectionChangedEventId:
      return "UIA_Text_TextSelectionChangedEventId";
    case UIA_Text_TextChangedEventId:
      return "UIA_Text_TextChangedEventId";
    case UIA_Window_WindowOpenedEventId:
      return "UIA_Window_WindowOpenedEventId";
    case UIA_Window_WindowClosedEventId:
      return "UIA_Window_WindowClosedEventId";
    case UIA_MenuModeStartEventId:
      return "UIA_MenuModeStartEventId";
    case UIA_MenuModeEndEventId:
      return "UIA_MenuModeEndEventId";
    case UIA_InputReachedTargetEventId:
      return "UIA_InputReachedTargetEventId";
    case UIA_InputReachedOtherElementEventId:
      return "UIA_InputReachedOtherElementEventId";
    case UIA_InputDiscardedEventId:
      return "UIA_InputDiscardedEventId";
    case UIA_SystemAlertEventId:
      return "UIA_SystemAlertEventId";
    case UIA_LiveRegionChangedEventId:
      return "UIA_LiveRegionChangedEventId";
    case UIA_HostedFragmentRootsInvalidatedEventId:
      return "UIA_HostedFragmentRootsInvalidatedEventId";
    case UIA_Drag_DragStartEventId:
      return "UIA_Drag_DragStartEventId";
    case UIA_Drag_DragCancelEventId:
      return "UIA_Drag_DragCancelEventId";
    case UIA_Drag_DragCompleteEventId:
      return "UIA_Drag_DragCompleteEventId";
    case UIA_DropTarget_DragEnterEventId:
      return "UIA_DropTarget_DragEnterEventId";
    case UIA_DropTarget_DragLeaveEventId:
      return "UIA_DropTarget_DragLeaveEventId";
    case UIA_DropTarget_DroppedEventId:
      return "UIA_DropTarget_DroppedEventId";
    case UIA_TextEdit_TextChangedEventId:
      return "UIA_TextEdit_TextChangedEventId";
    case UIA_TextEdit_ConversionTargetChangedEventId:
      return "UIA_TextEdit_ConversionTargetChangedEventId";
  }
#endif  // DCHECK_IS_ON()
  return "";
}

const char* GetControlType(long control_type) {
#if DCHECK_IS_ON()
  switch (control_type) {
    case UIA_ButtonControlTypeId:
      return "UIA_ButtonControlTypeId";
    case UIA_CalendarControlTypeId:
      return "UIA_CalendarControlTypeId";
    case UIA_CheckBoxControlTypeId:
      return "UIA_CheckBoxControlTypeId";
    case UIA_ComboBoxControlTypeId:
      return "UIA_ComboBoxControlTypeId";
    case UIA_EditControlTypeId:
      return "UIA_EditControlTypeId";
    case UIA_HyperlinkControlTypeId:
      return "UIA_HyperlinkControlTypeId";
    case UIA_ImageControlTypeId:
      return "UIA_ImageControlTypeId";
    case UIA_ListItemControlTypeId:
      return "UIA_ListItemControlTypeId";
    case UIA_ListControlTypeId:
      return "UIA_ListControlTypeId";
    case UIA_MenuControlTypeId:
      return "UIA_MenuControlTypeId";
    case UIA_MenuBarControlTypeId:
      return "UIA_MenuBarControlTypeId";
    case UIA_MenuItemControlTypeId:
      return "UIA_MenuItemControlTypeId";
    case UIA_ProgressBarControlTypeId:
      return "UIA_ProgressBarControlTypeId";
    case UIA_RadioButtonControlTypeId:
      return "UIA_RadioButtonControlTypeId";
    case UIA_ScrollBarControlTypeId:
      return "UIA_ScrollBarControlTypeId";
    case UIA_SliderControlTypeId:
      return "UIA_SliderControlTypeId";
    case UIA_SpinnerControlTypeId:
      return "UIA_SpinnerControlTypeId";
    case UIA_StatusBarControlTypeId:
      return "UIA_StatusBarControlTypeId";
    case UIA_TabControlTypeId:
      return "UIA_TabControlTypeId";
    case UIA_TabItemControlTypeId:
      return "UIA_TabItemControlTypeId";
    case UIA_TextControlTypeId:
      return "UIA_TextControlTypeId";
    case UIA_ToolBarControlTypeId:
      return "UIA_ToolBarControlTypeId";
    case UIA_ToolTipControlTypeId:
      return "UIA_ToolTipControlTypeId";
    case UIA_TreeControlTypeId:
      return "UIA_TreeControlTypeId";
    case UIA_TreeItemControlTypeId:
      return "UIA_TreeItemControlTypeId";
    case UIA_CustomControlTypeId:
      return "UIA_CustomControlTypeId";
    case UIA_GroupControlTypeId:
      return "UIA_GroupControlTypeId";
    case UIA_ThumbControlTypeId:
      return "UIA_ThumbControlTypeId";
    case UIA_DataGridControlTypeId:
      return "UIA_DataGridControlTypeId";
    case UIA_DataItemControlTypeId:
      return "UIA_DataItemControlTypeId";
    case UIA_DocumentControlTypeId:
      return "UIA_DocumentControlTypeId";
    case UIA_SplitButtonControlTypeId:
      return "UIA_SplitButtonControlTypeId";
    case UIA_WindowControlTypeId:
      return "UIA_WindowControlTypeId";
    case UIA_PaneControlTypeId:
      return "UIA_PaneControlTypeId";
    case UIA_HeaderControlTypeId:
      return "UIA_HeaderControlTypeId";
    case UIA_HeaderItemControlTypeId:
      return "UIA_HeaderItemControlTypeId";
    case UIA_TableControlTypeId:
      return "UIA_TableControlTypeId";
    case UIA_TitleBarControlTypeId:
      return "UIA_TitleBarControlTypeId";
    case UIA_SeparatorControlTypeId:
      return "UIA_SeparatorControlTypeId";
    case UIA_SemanticZoomControlTypeId:
      return "UIA_SemanticZoomControlTypeId";
    case UIA_AppBarControlTypeId:
      return "UIA_AppBarControlTypeId";
  }
#endif  // DCHECK_IS_ON()
  return "";
}


// SettingsAppMonitor::Context::EventHandler -----------------------------------

SettingsAppMonitor::Context::EventHandler::EventHandler() = default;

SettingsAppMonitor::Context::EventHandler::~EventHandler() = default;

void SettingsAppMonitor::Context::EventHandler::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> context_runner,
    const base::WeakPtr<SettingsAppMonitor::Context>& context,
    base::win::ScopedComPtr<IUIAutomation> automation) {
  context_runner_ = std::move(context_runner);
  context_ = context;
  automation_ = automation;
}

HRESULT SettingsAppMonitor::Context::EventHandler::HandleAutomationEvent(
    IUIAutomationElement* sender,
    EVENTID event_id) {
  DVLOG(1) << "event id: " << GetEventName(event_id) << ", automation id: "
           << GetCachedBstrValue(sender, UIA_AutomationIdPropertyId)
           << ", name: " << GetCachedBstrValue(sender, UIA_NamePropertyId)
           << ", control type: " << GetControlType(GetCachedInt32Value(
                                        sender, UIA_ControlTypePropertyId))
           << ", is peripheral: "
           << GetCachedBoolValue(sender, UIA_IsPeripheralPropertyId)
           << ", class name: "
           << GetCachedBstrValue(sender, UIA_ClassNamePropertyId)
           << ", pid: " << GetCachedInt32Value(sender, UIA_ProcessIdPropertyId)
           << ", value: "
           << GetCachedBstrValue(sender, UIA_ValueValuePropertyId)
           << ", runtime id: " << IntArrayToString(GetCachedInt32ArrayValue(
                                      sender, UIA_RuntimeIdPropertyId));

  switch (DetectElementType(automation_.get(), sender)) {
    case ElementType::DEFAULT_BROWSER:
      context_runner_->PostTask(
          FROM_HERE,
          base::Bind(&SettingsAppMonitor::Context::HandleChooserInvoked,
                     context_));
      break;
    case ElementType::BROWSER_BUTTON: {
      base::string16 browser_name(
          GetCachedBstrValue(sender, UIA_NamePropertyId));
      if (!browser_name.empty()) {
        context_runner_->PostTask(
            FROM_HERE,
            base::Bind(&SettingsAppMonitor::Context::HandleBrowserChosen,
                       context_, browser_name));
      }
      break;
    }
    case ElementType::SWITCH_ANYWAY:
      context_runner_->PostTask(
          FROM_HERE,
          base::Bind(&SettingsAppMonitor::Context::HandlePromoChoiceMade,
                     context_, false));
      break;
    case ElementType::CHECK_IT_OUT:
      context_runner_->PostTask(
          FROM_HERE,
          base::Bind(&SettingsAppMonitor::Context::HandlePromoChoiceMade,
                     context_, true));
      break;
    case ElementType::UNKNOWN:
      break;
  }

  return S_OK;
}

HRESULT SettingsAppMonitor::Context::EventHandler::HandleFocusChangedEvent(
    IUIAutomationElement* sender) {
  DVLOG(1) << "focus changed for automation id: "
           << GetCachedBstrValue(sender, UIA_AutomationIdPropertyId)
           << ", name: " << GetCachedBstrValue(sender, UIA_NamePropertyId)
           << ", control type: " << GetControlType(GetCachedInt32Value(
                                        sender, UIA_ControlTypePropertyId))
           << ", is peripheral: "
           << GetCachedBoolValue(sender, UIA_IsPeripheralPropertyId)
           << ", class name: "
           << GetCachedBstrValue(sender, UIA_ClassNamePropertyId)
           << ", pid: " << GetCachedInt32Value(sender, UIA_ProcessIdPropertyId)
           << ", value: "
           << GetCachedBstrValue(sender, UIA_ValueValuePropertyId)
           << ", runtime id: " << IntArrayToString(GetCachedInt32ArrayValue(
                                      sender, UIA_RuntimeIdPropertyId));
  context_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SettingsAppMonitor::Context::HandleFocusChangedEvent,
                 context_,
                 base::win::ScopedComPtr<IUIAutomationElement>(sender)));

  return S_OK;
}


// SettingsAppMonitor::Context -------------------------------------------------

base::WeakPtr<SettingsAppMonitor::Context>
SettingsAppMonitor::Context::Create() {
  Context* context = new Context();
  return context->weak_ptr_factory_.GetWeakPtr();
}

void SettingsAppMonitor::Context::DeleteOnAutomationThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  delete this;
}

void SettingsAppMonitor::Context::Initialize(
    base::SingleThreadTaskRunner* task_runner,
    scoped_refptr<base::SequencedTaskRunner> monitor_runner,
    const base::WeakPtr<SettingsAppMonitor>& monitor) {
  // This and all other methods must be called on the automation thread.
  DCHECK(task_runner->BelongsToCurrentThread());
  DCHECK(!monitor_runner->RunsTasksOnCurrentThread());

  task_runner_ = task_runner;
  monitor_runner_ = monitor_runner;
  monitor_ = monitor;

  HRESULT result = automation_.CreateInstance(CLSID_CUIAutomation, nullptr,
                                              CLSCTX_INPROC_SERVER);
  if (SUCCEEDED(result))
    result = automation_ ? InstallObservers() : E_FAIL;

  // Tell the monitor that initialization is complete one way or the other.
  monitor_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SettingsAppMonitor::OnInitialized, monitor_, result));

  // Self-destruct immediately if initialization failed to reduce overhead.
  if (FAILED(result))
    delete this;
}

SettingsAppMonitor::Context::Context() : weak_ptr_factory_(this) {}

SettingsAppMonitor::Context::~Context() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (event_handler_) {
    event_handler_.Release();
    automation_->RemoveAllEventHandlers();
  }
}

void SettingsAppMonitor::Context::HandleFocusChangedEvent(
    base::win::ScopedComPtr<IUIAutomationElement> sender) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Duplicate focus changed events are suppressed.
  ElementType element_type = DetectElementType(automation_.get(), sender.get());
  if (last_focused_element_ == element_type)
    return;
  last_focused_element_ = element_type;

  if (element_type == ElementType::DEFAULT_BROWSER) {
    monitor_runner_->PostTask(
        FROM_HERE, base::Bind(&SettingsAppMonitor::OnAppFocused, monitor_));
  } else if (element_type == ElementType::CHECK_IT_OUT) {
    monitor_runner_->PostTask(
        FROM_HERE, base::Bind(&SettingsAppMonitor::OnPromoFocused, monitor_));
  }
}

void SettingsAppMonitor::Context::HandleChooserInvoked() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  monitor_runner_->PostTask(
      FROM_HERE, base::Bind(&SettingsAppMonitor::OnChooserInvoked, monitor_));
}

void SettingsAppMonitor::Context::HandleBrowserChosen(
    const base::string16& browser_name) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  monitor_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SettingsAppMonitor::OnBrowserChosen, monitor_, browser_name));
}

void SettingsAppMonitor::Context::HandlePromoChoiceMade(bool accept_promo) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  monitor_runner_->PostTask(
      FROM_HERE, base::Bind(&SettingsAppMonitor::OnPromoChoiceMade, monitor_,
                            accept_promo));
}

base::win::ScopedComPtr<IUnknown>
SettingsAppMonitor::Context::GetEventHandler() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!event_handler_) {
    ATL::CComObject<EventHandler>* obj = nullptr;
    HRESULT result = ATL::CComObject<EventHandler>::CreateInstance(&obj);
    if (SUCCEEDED(result)) {
      obj->Initialize(task_runner_, weak_ptr_factory_.GetWeakPtr(),
                      automation_);
      obj->QueryInterface(event_handler_.Receive());
    }
  }
  return event_handler_;
}

base::win::ScopedComPtr<IUIAutomationEventHandler>
SettingsAppMonitor::Context::GetAutomationEventHandler() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::win::ScopedComPtr<IUIAutomationEventHandler> handler;
  handler.QueryFrom(GetEventHandler().get());
  return handler;
}

base::win::ScopedComPtr<IUIAutomationFocusChangedEventHandler>
SettingsAppMonitor::Context::GetFocusChangedEventHandler() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::win::ScopedComPtr<IUIAutomationFocusChangedEventHandler> handler;
  handler.QueryFrom(GetEventHandler().get());
  return handler;
}

HRESULT SettingsAppMonitor::Context::InstallObservers() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(automation_);

  // Create a cache request so that elements received by way of events contain
  // all data needed for procesing.
  base::win::ScopedComPtr<IUIAutomationCacheRequest> cache_request;
  HRESULT result = automation_->CreateCacheRequest(cache_request.Receive());
  if (FAILED(result))
    return result;
  ConfigureCacheRequest(cache_request.get());

  // Observe changes in focus.
  result = automation_->AddFocusChangedEventHandler(
      cache_request.get(), GetFocusChangedEventHandler().get());
  if (FAILED(result))
    return result;

  // Observe invocations.
  base::win::ScopedComPtr<IUIAutomationElement> desktop;
  result = automation_->GetRootElement(desktop.Receive());
  if (desktop) {
    result = automation_->AddAutomationEventHandler(
        UIA_Invoke_InvokedEventId, desktop.get(), TreeScope_Subtree,
        cache_request.get(), GetAutomationEventHandler().get());
  }

  return result;
}


// SettingsAppMonitor ----------------------------------------------------------

SettingsAppMonitor::SettingsAppMonitor(Delegate* delegate)
    : delegate_(delegate),
      automation_thread_("SettingsAppMonitorAutomation"),
      weak_ptr_factory_(this) {
  ui::win::CreateATLModuleIfNeeded();
  // Start the automation thread and initialize the automation client on it.
  context_ = Context::Create();
  automation_thread_.init_com_with_mta(true);
  automation_thread_.Start();
  automation_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SettingsAppMonitor::Context::Initialize, context_,
                 base::Unretained(automation_thread_.task_runner().get()),
                 base::SequencedTaskRunnerHandle::Get(),
                 weak_ptr_factory_.GetWeakPtr()));
}

SettingsAppMonitor::~SettingsAppMonitor() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // context_ is still valid when the caller destroys the instance before the
  // callback(s) have fired. In this case, delete the context on the automation
  // thread before joining with it. DeleteSoon is not used because the monitor
  // has only a WeakPtr to the context that is bound to the automation thread.
  automation_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&SettingsAppMonitor::Context::DeleteOnAutomationThread,
                 context_));
}

void SettingsAppMonitor::OnInitialized(HRESULT result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_->OnInitialized(result);
}

void SettingsAppMonitor::OnAppFocused() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_->OnAppFocused();
}

void SettingsAppMonitor::OnChooserInvoked() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_->OnChooserInvoked();
}

void SettingsAppMonitor::OnBrowserChosen(const base::string16& browser_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_->OnBrowserChosen(browser_name);
}

void SettingsAppMonitor::OnPromoFocused() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_->OnPromoFocused();
}

void SettingsAppMonitor::OnPromoChoiceMade(bool accept_promo) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_->OnPromoChoiceMade(accept_promo);
}

}  // namespace win
}  // namespace shell_integration
