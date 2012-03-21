// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_OFFSCREEN_TABS_OFFSCREEN_TABS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_OFFSCREEN_TABS_OFFSCREEN_TABS_API_H_
#pragma once

#include <string>

#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

// The offscreen tabs module depends on the tabs module so we can share
// code between them. The long-term goal is to fold the offscreen tabs
// functionality into the tabs API. While these methods seem very similar to
// those in tabs, there are a few key differences that need to be resolved.
//  - Offscreen tabs are invisible (maybe this could be a property on Tab).
//  - The lifetime of an offscreen tab is tied to the parent tab that opened
//    it. We do this to prevent leaking these tabs, since users wouldn't have
//    a way of directly closing them or knowing they're open.
//  - Offscreen tabs have a width and height, while regular tabs don't. This
//    lets clients control the dimensions of the images in ToDataUrl.

class BackingStore;
class SkBitmap;
class TabContentsWrapper;
namespace content {
class WebContents;
}  // namespace content

// Creates an offscreen tab.
class CreateOffscreenTabFunction : public SyncExtensionFunction {
 public:
  CreateOffscreenTabFunction();
 private:
  virtual ~CreateOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.create")
  DISALLOW_COPY_AND_ASSIGN(CreateOffscreenTabFunction);
};

// Gets info about an offscreen tab.
class GetOffscreenTabFunction : public SyncExtensionFunction {
 public:
  GetOffscreenTabFunction();
 private:
  virtual ~GetOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.get")
  DISALLOW_COPY_AND_ASSIGN(GetOffscreenTabFunction);
};

// Gets all offscreen tabs created by the tab that invoked this function.
class GetAllOffscreenTabFunction : public SyncExtensionFunction {
 public:
  GetAllOffscreenTabFunction();
 private:
  virtual ~GetAllOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.getAll")
  DISALLOW_COPY_AND_ASSIGN(GetAllOffscreenTabFunction);
};

// Removes an offscreen tab.
class RemoveOffscreenTabFunction : public SyncExtensionFunction {
 public:
  RemoveOffscreenTabFunction();
 private:
  virtual ~RemoveOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.remove")
  DISALLOW_COPY_AND_ASSIGN(RemoveOffscreenTabFunction);
};

// Synthesizes a keyboard event based on a passed-in JavaScript keyboard event.
// TODO(jstritar): This would be useful on the chrome.tabs API.
class SendKeyboardEventOffscreenTabFunction : public SyncExtensionFunction {
 public:
  SendKeyboardEventOffscreenTabFunction();
 private:
  virtual ~SendKeyboardEventOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
    "experimental.offscreenTabs.sendKeyboardEvent")
  DISALLOW_COPY_AND_ASSIGN(SendKeyboardEventOffscreenTabFunction);
};

// Synthesizes a mouse event based on a passed-in JavaScript mouse event.
// Since only the application knows where the user clicks, x and y coordinates
// need to be passed in as well in this case of click, mousedown, and mouseup.
// TODO(jstritar): This would be useful on the chrome.tabs API.
class SendMouseEventOffscreenTabFunction : public SyncExtensionFunction {
 public:
  SendMouseEventOffscreenTabFunction();
 private:
  virtual ~SendMouseEventOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.sendMouseEvent")
  DISALLOW_COPY_AND_ASSIGN(SendMouseEventOffscreenTabFunction);
};

// Gets a snapshot of the offscreen tab and returns it as a data URL.
class ToDataUrlOffscreenTabFunction : public CaptureVisibleTabFunction {
 public:
  ToDataUrlOffscreenTabFunction();
 private:
  virtual ~ToDataUrlOffscreenTabFunction();
  virtual bool GetTabToCapture(content::WebContents** web_contents,
                               TabContentsWrapper** wrapper) OVERRIDE;
  // TODO(jstritar): Rename to toDataUrl.
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.toDataUrl")
  DISALLOW_COPY_AND_ASSIGN(ToDataUrlOffscreenTabFunction);
};

// Updates an offscreen tab.
class UpdateOffscreenTabFunction : public UpdateTabFunction {
 public:
  UpdateOffscreenTabFunction();
 private:
  virtual ~UpdateOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;
  virtual void PopulateResult() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.update")
  DISALLOW_COPY_AND_ASSIGN(UpdateOffscreenTabFunction);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_OFFSCREEN_TABS_OFFSCREEN_TABS_API_H_
