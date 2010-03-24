// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_popup_api.h"

#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_dom_ui.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/url_constants.h"
#include "gfx/point.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/views/extensions/extension_popup.h"
#include "views/view.h"
#include "views/focus/focus_manager.h"
#endif  // TOOLKIT_VIEWS

namespace extension_popup_module_events {

const char kOnPopupClosed[] = "experimental.popup.onClosed.%d";

}  // namespace extension_popup_module_events

namespace {

// Errors.
const char kBadAnchorArgument[] = "Invalid anchor argument.";
const char kInvalidURLError[] = "Invalid URL.";
const char kNotAnExtension[] = "Not an extension view.";

// Keys.
const wchar_t kUrlKey[] = L"url";
const wchar_t kWidthKey[] = L"width";
const wchar_t kHeightKey[] = L"height";
const wchar_t kTopKey[] = L"top";
const wchar_t kLeftKey[] = L"left";
const wchar_t kGiveFocusKey[] = L"giveFocus";
const wchar_t kDomAnchorKey[] = L"domAnchor";
const wchar_t kBorderStyleKey[] = L"borderStyle";

// chrome enumeration values
const char kRectangleChrome[] = "rectangle";

};  // namespace

#if defined(TOOLKIT_VIEWS)
// ExtensionPopupHost objects implement the environment necessary to host
// an ExtensionPopup views for the popup api. Its main job is to handle
// its lifetime and to fire the popup-closed event when the popup is closed.
// Because the close-on-focus-lost behavior is different from page action
// and browser action, it also manages its own focus change listening. The
// difference in close-on-focus-lost is that in the page action and browser
// action cases, the popup closes when the focus leaves the popup or any of its
// children. In this case, the popup closes when the focus leaves the popups
// containing view or any of *its* children.
class ExtensionPopupHost : public ExtensionPopup::Observer,
                           public views::WidgetFocusChangeListener,
                           public base::RefCounted<ExtensionPopupHost> {
 public:
  explicit ExtensionPopupHost(ExtensionFunctionDispatcher* dispatcher)
      : dispatcher_(dispatcher), popup_(NULL) {
    AddRef();  // Balanced in DispatchPopupClosedEvent().
    views::FocusManager::GetWidgetFocusManager()->AddFocusChangeListener(this);
  }

  ~ExtensionPopupHost() {
    views::FocusManager::GetWidgetFocusManager()->
        RemoveFocusChangeListener(this);
  }

  void set_popup(ExtensionPopup* popup) {
    popup_ = popup;
  }

  // Overriden from ExtensionPopup::Observer
  virtual void ExtensionPopupClosed(ExtensionPopup* popup) {
    // The OnPopupClosed event should be sent later to give the popup time to
    // complete closing.
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &ExtensionPopupHost::DispatchPopupClosedEvent));
  }

  virtual void ExtensionHostCreated(ExtensionHost* host) {
    // Pop-up views should share the same automation routing configuration as
    // their hosting views, so register the RenderViewHost of the pop-up with
    // the AutomationResourceRoutingDelegate interface of the dispatcher.
    AutomationResourceRoutingDelegate* router =
        GetRoutingFromDispatcher(dispatcher_);
    if (router)
      router->RegisterRenderViewHost(host->render_view_host());
  }

  virtual void DispatchPopupClosedEvent() {
    // Unregister the automation resource routing registered upon host
    // creation.
    AutomationResourceRoutingDelegate* router =
        GetRoutingFromDispatcher(dispatcher_);
    if (router)
      router->UnregisterRenderViewHost(popup_->host()->render_view_host());

    PopupEventRouter::OnPopupClosed(
        dispatcher_->profile(), dispatcher_->render_view_host()->routing_id());
    dispatcher_ = NULL;
    Release();  // Balanced in ctor.
  }

  // Overriden from views::WidgetFocusChangeListener
  virtual void NativeFocusWillChange(gfx::NativeView focused_before,
                                     gfx::NativeView focused_now) {
    // If no view is to be focused, then Chrome was deactivated, so hide the
    // popup.
    if (focused_now) {
      gfx::NativeView host_view =
          dispatcher_->delegate()->GetNativeViewOfHost();

      // If the widget hosting the popup contains the newly focused view, then
      // don't dismiss the pop-up.
      views::Widget* popup_root_widget = popup_->host()->view()->GetWidget();
      if (popup_root_widget &&
          popup_root_widget->ContainsNativeView(focused_now))
        return;

      // If the widget or RenderWidgetHostView hosting the extension that
      // launched the pop-up is receiving focus, then don't dismiss the popup.
      views::Widget* host_widget =
          views::Widget::GetWidgetFromNativeView(host_view);
      if (host_widget && host_widget->ContainsNativeView(focused_now))
        return;

      RenderWidgetHostView* render_host_view =
          RenderWidgetHostView::GetRenderWidgetHostViewFromNativeView(
              host_view);
      if (render_host_view &&
          render_host_view->ContainsNativeView(focused_now))
        return;
    }

    // We are careful here to let the current event loop unwind before
    // causing the popup to be closed.
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(popup_,
        &ExtensionPopup::Close));
  }

 private:
  // Returns the AutomationResourceRoutingDelegate interface for |dispatcher|.
  static AutomationResourceRoutingDelegate*
      GetRoutingFromDispatcher(ExtensionFunctionDispatcher* dispatcher) {
    if (!dispatcher)
      return NULL;

    RenderViewHost* render_view_host = dispatcher->render_view_host();
    RenderViewHostDelegate* delegate =
        render_view_host ? render_view_host->delegate() : NULL;

    return delegate ? delegate->GetAutomationResourceRoutingDelegate() : NULL;
  }

  // A pointer to the dispatcher that handled the request that opened this
  // popup view.
  ExtensionFunctionDispatcher* dispatcher_;

  // A pointer to the popup.
  ExtensionPopup* popup_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopupHost);
};
#endif  // TOOLKIT_VIEWS

PopupShowFunction::PopupShowFunction()
#if defined (TOOLKIT_VIEWS)
    : popup_(NULL)
#endif
{}

void PopupShowFunction::Run() {
#if defined(TOOLKIT_VIEWS)
  if (!RunImpl()) {
    SendResponse(false);
  } else {
    // If the contents of the popup are already available, then immediately
    // send the response.  Otherwise wait for the EXTENSION_POPUP_VIEW_READY
    // notification.
    if (popup_->host() && popup_->host()->document_element_available()) {
      SendResponse(true);
    } else {
      AddRef();
      registrar_.Add(this, NotificationType::EXTENSION_POPUP_VIEW_READY,
                     NotificationService::AllSources());
      registrar_.Add(this, NotificationType::EXTENSION_HOST_DESTROYED,
                     NotificationService::AllSources());
    }
  }
#else
  SendResponse(false);
#endif
}

bool PopupShowFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = args_as_list();

  std::string url_string;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(0, &url_string));

  DictionaryValue* show_details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &show_details));

  DictionaryValue* dom_anchor = NULL;
  EXTENSION_FUNCTION_VALIDATE(show_details->GetDictionary(kDomAnchorKey,
                                                          &dom_anchor));

  int dom_top, dom_left;
  EXTENSION_FUNCTION_VALIDATE(dom_anchor->GetInteger(kTopKey,
                                                     &dom_top));
  EXTENSION_FUNCTION_VALIDATE(dom_anchor->GetInteger(kLeftKey,
                                                     &dom_left));

  int dom_width, dom_height;
  EXTENSION_FUNCTION_VALIDATE(dom_anchor->GetInteger(kWidthKey,
                                                     &dom_width));
  EXTENSION_FUNCTION_VALIDATE(dom_anchor->GetInteger(kHeightKey,
                                                     &dom_height));
  EXTENSION_FUNCTION_VALIDATE(dom_top >= 0 && dom_left >= 0 &&
                              dom_width >= 0 && dom_height >= 0);

  // The default behaviour is to give the focus to the pop-up window.
  bool give_focus = true;
  if (show_details->HasKey(kGiveFocusKey)) {
    EXTENSION_FUNCTION_VALIDATE(show_details->GetBoolean(kGiveFocusKey,
                                                         &give_focus));
  }

#if defined(TOOLKIT_VIEWS)
  // The default behaviour is to provide the bubble-chrome to the popup.
  ExtensionPopup::PopupChrome chrome = ExtensionPopup::BUBBLE_CHROME;
  if (show_details->HasKey(kBorderStyleKey)) {
    std::string chrome_string;
    EXTENSION_FUNCTION_VALIDATE(show_details->GetString(kBorderStyleKey,
                                                        &chrome_string));
    if (chrome_string == kRectangleChrome)
      chrome = ExtensionPopup::RECTANGLE_CHROME;
  }
#endif

  GURL url = dispatcher()->url().Resolve(url_string);
  if (!url.is_valid()) {
    error_ = kInvalidURLError;
    return false;
  }

  // Disallow non-extension requests, or requests outside of the requesting
  // extension view's extension.
  const std::string& extension_id = url.host();
  if (extension_id != dispatcher()->GetExtension()->id() ||
      !url.SchemeIs(chrome::kExtensionScheme)) {
    error_ = kInvalidURLError;
    return false;
  }

#if defined(TOOLKIT_VIEWS)
  gfx::Point origin(dom_left, dom_top);
  if (!dispatcher()->render_view_host()->view()) {
    error_ = kNotAnExtension;
    return false;
  }

  gfx::Rect content_bounds =
      dispatcher()->render_view_host()->view()->GetViewBounds();
  origin.Offset(content_bounds.x(), content_bounds.y());
  gfx::Rect rect(origin.x(), origin.y(), dom_width, dom_height);

  // Pop-up from extension views (ExtensionShelf, etc.), and drop-down when
  // in a TabContents view.
  ViewType::Type view_type =
      dispatcher()->render_view_host()->delegate()->GetRenderViewType();
  BubbleBorder::ArrowLocation arrow_location =
      view_type == ViewType::TAB_CONTENTS ?
          BubbleBorder::TOP_LEFT : BubbleBorder::BOTTOM_LEFT;

  // Get the correct native window to pass to ExtensionPopup.
  // ExtensionFunctionDispatcher::Delegate may provide a custom implementation
  // of this.
  gfx::NativeWindow window =
      dispatcher()->delegate()->GetCustomFrameNativeWindow();
  if (!window)
    window = GetCurrentBrowser()->window()->GetNativeHandle();

  // ExtensionPopupHost manages it's own lifetime.
  ExtensionPopupHost* popup_host = new ExtensionPopupHost(dispatcher());
  popup_ = ExtensionPopup::Show(url,
                                GetCurrentBrowser(),
                                dispatcher()->profile(),
                                window,
                                rect,
                                arrow_location,
                                give_focus,
                                false,  // inspect_with_devtools
                                chrome,
                                popup_host);  // ExtensionPopup::Observer

  // popup_host will handle focus change listening and close the popup when
  // focus leaves the containing views hierarchy.
  popup_->set_close_on_lost_focus(false);
  popup_host->set_popup(popup_);
#endif  // defined(TOOLKIT_VIEWS)
  return true;
}

void PopupShowFunction::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
#if defined(TOOLKIT_VIEWS)
  DCHECK(type == NotificationType::EXTENSION_POPUP_VIEW_READY ||
         type == NotificationType::EXTENSION_HOST_DESTROYED);
  DCHECK(popup_ != NULL);

  if (popup_ && type == NotificationType::EXTENSION_POPUP_VIEW_READY &&
      Details<ExtensionHost>(popup_->host()) == details) {
    SendResponse(true);
    Release();  // Balanced in Run().
  } else if (popup_ && type == NotificationType::EXTENSION_HOST_DESTROYED &&
             Details<ExtensionHost>(popup_->host()) == details) {
    // If the host was destroyed, then report failure, and release the remaining
    // reference.
    SendResponse(false);
    Release();  // Balanced in Run().
  }
#endif  // defined(TOOLKIT_VIEWS)
}

// static
void PopupEventRouter::OnPopupClosed(Profile* profile,
                                     int routing_id) {
  std::string full_event_name = StringPrintf(
      extension_popup_module_events::kOnPopupClosed,
      routing_id);

  profile->GetExtensionMessageService()->DispatchEventToRenderers(
      full_event_name,
      base::JSONWriter::kEmptyArray,
      profile->IsOffTheRecord());
}
