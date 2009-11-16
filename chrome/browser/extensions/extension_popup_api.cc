// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_popup_api.h"

#include "base/gfx/point.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/browser/extensions/extension_dom_ui.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/extensions/extension_popup.h"
#include "views/view.h"
#endif

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

};  // namespace

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

  DictionaryValue* dom_anchor = NULL;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &dom_anchor));

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

  GURL url = dispatcher()->url().Resolve(url_string);
  if (!url.is_valid()) {
    error_ = kInvalidURLError;
    return false;
  }

  // Disallow non-extension requests, or requests outside of the requesting
  // extension view's extension.
  const std::string& extension_id = url.host();
  if (extension_id != dispatcher()->GetExtension()->id() ||
      !url.SchemeIs("chrome-extension")) {
    error_ = kInvalidURLError;
    return false;
  }

#if defined(TOOLKIT_VIEWS)
  gfx::Point origin(dom_left, dom_top);
  if (!ConvertHostPointToScreen(&origin)) {
    error_ = kNotAnExtension;
    return false;
  }
  gfx::Rect rect(origin.x(), origin.y(), dom_width, dom_height);

  // Pop-up from extension views (ExtensionShelf, etc.), and drop-down when
  // in a TabContents view.
  BubbleBorder::ArrowLocation arrow_location =
      (NULL != dispatcher()->GetExtensionHost()) ? BubbleBorder::BOTTOM_LEFT :
                                                   BubbleBorder::TOP_LEFT;
  popup_ = ExtensionPopup::Show(url, dispatcher()->GetBrowser(), rect,
                                arrow_location);

  ExtensionPopupHost* popup_host = dispatcher()->GetPopupHost();
  DCHECK(popup_host);

  popup_host->set_child_popup(popup_);
  popup_->set_delegate(popup_host);
#endif  // defined(TOOLKIT_VIEWS)
  return true;
}

bool PopupShowFunction::ConvertHostPointToScreen(gfx::Point* point) {
  DCHECK(point);

  // If the popup is being requested from an ExtensionHost, then compute
  // the sreen coordinates based on the views::View object of the ExtensionHost.
  if (dispatcher()->GetExtensionHost()) {
    // A dispatcher cannot have both an ExtensionHost, and an ExtensionDOMUI.
    DCHECK(!dispatcher()->GetExtensionDOMUI());

#if defined(TOOLKIT_VIEWS)
    views::View* extension_view = dispatcher()->GetExtensionHost()->view();
    if (!extension_view)
      return false;

    views::View::ConvertPointToScreen(extension_view, point);
#else
    // TODO(port)
    NOTIMPLEMENTED();
#endif  // defined(TOOLKIT_VIEWS)
  } else if (dispatcher()->GetExtensionDOMUI()) {
    // Otherwise, the popup is being requested from a TabContents, so determine
    // the screen-space position through the TabContentsView.
    ExtensionDOMUI* dom_ui = dispatcher()->GetExtensionDOMUI();
    TabContents* tab_contents = dom_ui->tab_contents();
    if (!tab_contents)
      return false;

    gfx::Rect content_bounds;
    tab_contents->GetContainerBounds(&content_bounds);
    point->Offset(content_bounds.x(), content_bounds.y());
  }

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
      base::JSONWriter::kEmptyArray);
}
