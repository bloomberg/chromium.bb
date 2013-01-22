// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_CONTEXT_MENU_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_CONTEXT_MENU_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/window_open_disposition.h"

namespace base {
class ListValue;
}

// The handler for JavaScript messages related to the context menus.
// It's the job of the actual HTML page to intercept the contextmenu event,
// disable it and show its own custom menu.
class ContextMenuHandler : public content::WebUIMessageHandler {
 public:
  ContextMenuHandler();
  virtual ~ContextMenuHandler();

  // WebUIMessageHandler override and implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Invoked (by on_item_selected_callback_) when an item has been selected.
  void OnItemSelected(int item_id);

  // Callback for the "showContextMenu" message.
  void HandleShowContextMenu(const base::ListValue* args);

  // Below are the message that are so far only triggered by the context menu.
  // They should be moved to other files if they become used from other places.

  // Callback for the "openInNewTab" message.
  void HandleOpenInNewTab(const base::ListValue* args);

  // Callback for the "openInIncognitoTab" message.
  void HandleOpenInIncognitoTab(const base::ListValue* args);

 private:
  // Opens the URL stored as the first value of |args| with the given
  // |disposition|.  The URL will always be opened as an AUTO_BOOKMARK.
  void OpenUrl(const base::ListValue* args, WindowOpenDisposition disposition);

  // Used to get a WeakPtr to self on the UI thread.
  base::WeakPtrFactory<ContextMenuHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_ANDROID_CONTEXT_MENU_HANDLER_H_
