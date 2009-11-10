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
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
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

  DictionaryValue* popup_info = NULL;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(0, &popup_info));

  std::string url_string;
  EXTENSION_FUNCTION_VALIDATE(popup_info->GetString(kUrlKey,
                                                    &url_string));

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
  views::View* extension_view = dispatcher()->GetExtensionHost()->view();
  gfx::Point origin(dom_left, dom_top);
  views::View::ConvertPointToScreen(extension_view, &origin);
  gfx::Rect rect(origin.x(), origin.y(), dom_width, dom_height);

  popup_ = ExtensionPopup::Show(url, dispatcher()->GetBrowser(), rect,
                                BubbleBorder::BOTTOM_LEFT);

  dispatcher()->GetExtensionHost()->set_child_popup(popup_);
  popup_->set_delegate(dispatcher()->GetExtensionHost());
#endif
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
#endif
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
