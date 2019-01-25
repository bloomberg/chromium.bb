// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/clipboard/clipboard_api.h"

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/event_router.h"
#include "ui/base/clipboard/clipboard_monitor.h"

namespace extensions {

namespace clipboard = api::clipboard;

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ClipboardAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ClipboardAPI>*
ClipboardAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

ClipboardAPI::ClipboardAPI(content::BrowserContext* context)
    : browser_context_(context) {
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);
}

ClipboardAPI::~ClipboardAPI() {
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
}

void ClipboardAPI::OnClipboardDataChanged() {
  EventRouter* router = EventRouter::Get(browser_context_);
  if (router &&
      router->HasEventListener(clipboard::OnClipboardDataChanged::kEventName)) {
    std::unique_ptr<Event> event(
        new Event(events::CLIPBOARD_ON_CLIPBOARD_DATA_CHANGED,
                  clipboard::OnClipboardDataChanged::kEventName,
                  std::make_unique<base::ListValue>()));
    router->BroadcastEvent(std::move(event));
  }
}

ClipboardSetImageDataFunction::~ClipboardSetImageDataFunction() {}

ExtensionFunction::ResponseAction ClipboardSetImageDataFunction::Run() {
  std::unique_ptr<clipboard::SetImageData::Params> params(
      clipboard::SetImageData::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // Fill in the omitted additional data items with empty data.
  if (!params->additional_items)
    params->additional_items = std::make_unique<AdditionalDataItemList>();

  if (!IsAdditionalItemsParamValid(*params->additional_items)) {
    return RespondNow(Error("Unsupported additionalItems parameter data."));
  }

  ExtensionsAPIClient::Get()->SaveImageDataToClipboard(
      std::vector<char>(params->image_data.begin(), params->image_data.end()),
      params->type, std::move(*params->additional_items),
      base::Bind(&ClipboardSetImageDataFunction::OnSaveImageDataSuccess, this),
      base::Bind(&ClipboardSetImageDataFunction::OnSaveImageDataError, this));
  return RespondLater();
}

void ClipboardSetImageDataFunction::OnSaveImageDataSuccess() {
  Respond(NoArguments());
}

void ClipboardSetImageDataFunction::OnSaveImageDataError(
    const std::string& error) {
  Respond(Error(error));
}

bool ClipboardSetImageDataFunction::IsAdditionalItemsParamValid(
    const AdditionalDataItemList& items) {
  // Limit the maximum text/html data length to 2MB.
  const size_t max_item_data_bytes = 2097152;

  bool has_text_plain = false;
  bool has_text_html = false;
  for (const clipboard::AdditionalDataItem& item : items) {
    switch (item.type) {
      case clipboard::DATA_ITEM_TYPE_TEXTPLAIN:
        if (has_text_plain)
          return false;
        has_text_plain = true;
        break;
      case clipboard::DATA_ITEM_TYPE_TEXTHTML:
        if (has_text_html)
          return false;
        has_text_html = true;
        break;
      default:
        NOTREACHED();
    }
    // Check maximum length of the string data.
    if (item.data.length() > max_item_data_bytes)
      return false;
  }
  return true;
}

}  // namespace extensions
