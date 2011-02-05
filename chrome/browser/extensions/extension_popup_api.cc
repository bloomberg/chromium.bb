// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_popup_api.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/url_constants.h"
#include "ui/gfx/point.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/bubble_border.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
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
const char kPopupsDisallowed[] =
    "Popups are only supported from tab-contents views.";

// Keys.
const char kWidthKey[] = "width";
const char kHeightKey[] = "height";
const char kTopKey[] = "top";
const char kLeftKey[] = "left";
const char kGiveFocusKey[] = "giveFocus";
const char kDomAnchorKey[] = "domAnchor";
const char kBorderStyleKey[] = "borderStyle";
const char kMaxSizeKey[] = "maxSize";

// chrome enumeration values
const char kRectangleChrome[] = "rectangle";

#if defined(TOOLKIT_VIEWS)
// Returns an updated arrow location, conditioned on the type of intersection
// between the popup window, and the screen.  |location| is the current position
// of the arrow on the popup.  |intersection| is the rect representing the
// intersection between the popup view and its working screen.  |popup_rect|
// is the rect of the popup window in screen space coordinates.
// The returned location will be horizontally or vertically inverted based on
// if the popup has been clipped horizontally or vertically.
BubbleBorder::ArrowLocation ToggleArrowLocation(
    BubbleBorder::ArrowLocation location, const gfx::Rect& intersection,
    const gfx::Rect& popup_rect) {
  // If the popup has been clipped horizontally, flip the right-left position
  // of the arrow.
  if (intersection.right() != popup_rect.right() ||
      intersection.x() != popup_rect.x()) {
    location = BubbleBorder::horizontal_mirror(location);
  }

  // If the popup has been clipped vertically, flip the bottom-top position
  // of the arrow.
  if (intersection.y() != popup_rect.y() ||
      intersection.bottom() != popup_rect.bottom()) {
    location = BubbleBorder::vertical_mirror(location);
  }

  return location;
}
#endif  // TOOLKIT_VIEWS

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
                           public base::RefCounted<ExtensionPopupHost>,
                           public NotificationObserver {
 public:
  // Pass |max_popup_size| to specify the maximal size to which the popup
  // will expand.  A width or height of 0 will result in the popup making use
  // of the default max width or height, respectively: ExtensionPopup:kMaxWidth,
  // and ExtensionPopup::kMaxHeight.
  explicit ExtensionPopupHost(ExtensionFunctionDispatcher* dispatcher,
                              const gfx::Size& max_popup_size)
      : dispatcher_(dispatcher), popup_(NULL), max_popup_size_(max_popup_size) {
    AddRef();  // Balanced in DispatchPopupClosedEvent().
    views::FocusManager::GetWidgetFocusManager()->AddFocusChangeListener(this);
  }

  ~ExtensionPopupHost() {
    views::FocusManager::GetWidgetFocusManager()->
        RemoveFocusChangeListener(this);
  }

  void set_popup(ExtensionPopup* popup) {
    popup_ = popup;

    // Now that a popup has been assigned, listen for subsequent popups being
    // created in the same extension - we want to disallow more than one
    // concurrently displayed popup windows.
    registrar_.Add(
        this,
        NotificationType::EXTENSION_HOST_CREATED,
        Source<ExtensionProcessManager>(
            dispatcher_->profile()->GetExtensionProcessManager()));

    registrar_.Add(
        this,
        NotificationType::RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
        Source<RenderViewHost>(dispatcher_->render_view_host()));

    registrar_.Add(
        this,
        NotificationType::EXTENSION_FUNCTION_DISPATCHER_DESTROYED,
        Source<Profile>(dispatcher_->profile()));
  }

  // Overridden from ExtensionPopup::Observer
  virtual void ExtensionPopupIsClosing(ExtensionPopup* popup) {
    // Unregister the automation resource routing registered upon host
    // creation.
    AutomationResourceRoutingDelegate* router =
        GetRoutingFromDispatcher(dispatcher_);
    if (router)
      router->UnregisterRenderViewHost(popup_->host()->render_view_host());
  }

  virtual void ExtensionPopupClosed(void* popup_token) {
    if (popup_ == popup_token) {
      popup_ = NULL;
      DispatchPopupClosedEvent();
    }
  }

  virtual void ExtensionHostCreated(ExtensionHost* host) {
    // Pop-up views should share the same automation routing configuration as
    // their hosting views, so register the RenderViewHost of the pop-up with
    // the AutomationResourceRoutingDelegate interface of the dispatcher.
    AutomationResourceRoutingDelegate* router =
        GetRoutingFromDispatcher(dispatcher_);
    if (router)
      router->RegisterRenderViewHost(host->render_view_host());

    // Extension hosts created for popup contents exist in the same tab
    // contents as the ExtensionFunctionDispatcher that requested the popup.
    // For example, '_blank' link navigation should be routed through the tab
    // contents that requested the popup.
    if (dispatcher_ && dispatcher_->delegate()) {
      host->set_associated_tab_contents(
          dispatcher_->delegate()->associated_tab_contents());
    }
  }

  virtual void ExtensionPopupCreated(ExtensionPopup* popup) {
    // The popup has been created, but not yet displayed, so install the max
    // size overrides before the first positioning.
    if (max_popup_size_.width())
      popup->set_max_width(max_popup_size_.width());

    if (max_popup_size_.height())
      popup->set_max_height(max_popup_size_.height());
  }

  virtual void ExtensionPopupResized(ExtensionPopup* popup) {
    // Reposition the location of the arrow on the popup so that the popup
    // better fits on the working monitor.
    gfx::Rect popup_rect = popup->GetOuterBounds();
    if (popup_rect.IsEmpty())
      return;

    scoped_ptr<WindowSizer::MonitorInfoProvider> monitor_provider(
        WindowSizer::CreateDefaultMonitorInfoProvider());
    gfx::Rect monitor_bounds(
        monitor_provider->GetMonitorWorkAreaMatching(popup_rect));
    gfx::Rect intersection = monitor_bounds.Intersect(popup_rect);

    // If the popup is totally out of the bounds of the monitor, then toggling
    // the arrow location will not result in an un-clipped window.
    if (intersection.IsEmpty())
      return;

    if (!intersection.Equals(popup_rect)) {
      // The popup was clipped by the monitor.  Toggle the arrow position
      // to see if that improves visibility.  Note:  The assignment and
      // re-assignment of the arrow-position will not trigger an intermittent
      // display.
      BubbleBorder::ArrowLocation previous_location = popup->arrow_position();
      BubbleBorder::ArrowLocation flipped_location = ToggleArrowLocation(
          previous_location, intersection, popup_rect);
      popup->SetArrowPosition(flipped_location);

      // Double check that toggling the position actually improved the
      // situation - the popup will be contained entirely in its working monitor
      // bounds.
      gfx::Rect flipped_bounds = popup->GetOuterBounds();
      gfx::Rect updated_monitor_bounds =
          monitor_provider->GetMonitorWorkAreaMatching(flipped_bounds);
      if (!updated_monitor_bounds.Contains(flipped_bounds))
        popup->SetArrowPosition(previous_location);
    }
  }

  // Overridden from views::WidgetFocusChangeListener
  virtual void NativeFocusWillChange(gfx::NativeView focused_before,
                                     gfx::NativeView focused_now) {
    // If the popup doesn't exist, then do nothing.
    if (!popup_)
      return;

    // If no view is to be focused, then Chrome was deactivated, so hide the
    // popup.
    if (focused_now) {
      // On XP, the focus change handler may be invoked when the delegate has
      // already been revoked.
      // TODO(twiz@chromium.org):  Resolve the trigger of this behaviour.
      if (!dispatcher_ || !dispatcher_->delegate())
        return;

      gfx::NativeView host_view =
          dispatcher_->delegate()->GetNativeViewOfHost();

      // If the widget hosting the popup contains the newly focused view, then
      // don't dismiss the pop-up.
      ExtensionView* view = popup_->host()->view();
      if (view) {
        views::Widget* popup_root_widget = view->GetWidget();
        if (popup_root_widget &&
            popup_root_widget->ContainsNativeView(focused_now))
          return;
      }

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

  // Overridden from NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (NotificationType::EXTENSION_HOST_CREATED == type) {
      Details<ExtensionHost> details_host(details);
      // Disallow multiple pop-ups from the same extension, by closing
      // the presently opened popup during construction of any new popups.
      if (ViewType::EXTENSION_POPUP == details_host->GetRenderViewType() &&
          popup_->host()->extension() == details_host->extension() &&
          Details<ExtensionHost>(popup_->host()) != details) {
        popup_->Close();
      }
    } else if (NotificationType::RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW ==
        type) {
      if (Source<RenderViewHost>(dispatcher_->render_view_host()) == source) {
        // If the parent render view is about to be closed, signal closure
        // of the popup.
        popup_->Close();
      }
    } else if (NotificationType::EXTENSION_FUNCTION_DISPATCHER_DESTROYED ==
        type) {
      // Popups should not outlive the dispatchers that launched them.
      // Normally, long-lived popups will be dismissed in response to the
      // RENDER_VIEW_WILL_CLOSE_BY_RENDER_VIEW_HOST message.  Unfortunately,
      // if the hosting view invokes window.close(), there is no communication
      // back to the browser until the entire view has been torn down, at which
      // time the dispatcher will be invoked.
      // Note:  The onClosed event will not be fired, but because the hosting
      // view has already been torn down, it is already too late to process it.
      // TODO(twiz):  Add a communication path between the renderer and browser
      // for RenderView closure notifications initiatied within the renderer.
      if (Details<ExtensionFunctionDispatcher>(dispatcher_) == details) {
        dispatcher_ = NULL;
        popup_->Close();
      }
    }
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

  void DispatchPopupClosedEvent() {
    if (dispatcher_) {
      PopupEventRouter::OnPopupClosed(
          dispatcher_->profile(),
          dispatcher_->render_view_host()->routing_id());
      dispatcher_ = NULL;
    }
    Release();  // Balanced in ctor.
  }

  // A pointer to the dispatcher that handled the request that opened this
  // popup view.
  ExtensionFunctionDispatcher* dispatcher_;

  // A pointer to the popup.
  ExtensionPopup* popup_;

  // The maximal size to which the popup is permitted to expand.
  gfx::Size max_popup_size_;

  NotificationRegistrar registrar_;

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
  // Popups may only be displayed from TAB_CONTENTS and EXTENSION_INFOBAR.
  ViewType::Type view_type =
      dispatcher()->render_view_host()->delegate()->GetRenderViewType();
  if (ViewType::TAB_CONTENTS != view_type &&
      ViewType::EXTENSION_INFOBAR != view_type) {
    error_ = kPopupsDisallowed;
    return false;
  }

  std::string url_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &url_string));

  DictionaryValue* show_details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &show_details));

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

  int max_width = 0;
  int max_height = 0;
  if (show_details->HasKey(kMaxSizeKey)) {
    DictionaryValue* max_size = NULL;
    EXTENSION_FUNCTION_VALIDATE(show_details->GetDictionary(kMaxSizeKey,
                                                            &max_size));

    if (max_size->HasKey(kWidthKey))
      EXTENSION_FUNCTION_VALIDATE(max_size->GetInteger(kWidthKey, &max_width));

    if (max_size->HasKey(kHeightKey))
      EXTENSION_FUNCTION_VALIDATE(max_size->GetInteger(kHeightKey,
                                                       &max_height));
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
  if (extension_id != GetExtension()->id() ||
      !url.SchemeIs(chrome::kExtensionScheme)) {
    error_ = kInvalidURLError;
    return false;
  }

  gfx::Point origin(dom_left, dom_top);
  if (!dispatcher()->render_view_host()->view()) {
    error_ = kNotAnExtension;
    return false;
  }

  gfx::Rect content_bounds =
      dispatcher()->render_view_host()->view()->GetViewBounds();
  origin.Offset(content_bounds.x(), content_bounds.y());
  gfx::Rect rect(origin.x(), origin.y(), dom_width, dom_height);

  // Get the correct native window to pass to ExtensionPopup.
  // ExtensionFunctionDispatcher::Delegate may provide a custom implementation
  // of this.
  gfx::NativeWindow window =
      dispatcher()->delegate()->GetCustomFrameNativeWindow();
  if (!window)
    window = GetCurrentBrowser()->window()->GetNativeHandle();

#if defined(TOOLKIT_VIEWS)
  BubbleBorder::ArrowLocation arrow_location = BubbleBorder::TOP_LEFT;

  // ExtensionPopupHost manages it's own lifetime.
  ExtensionPopupHost* popup_host =
      new ExtensionPopupHost(dispatcher(), gfx::Size(max_width, max_height));
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

  // Wait for notification that the popup view is ready (and onload has been
  // called), before completing the API call.
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
  std::string full_event_name = base::StringPrintf(
      extension_popup_module_events::kOnPopupClosed,
      routing_id);

  profile->GetExtensionEventRouter()->DispatchEventToRenderers(
      full_event_name, base::JSONWriter::kEmptyArray, profile, GURL());
}
