// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_OFFSCREEN_TABS_OFFSCREEN_TABS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_OFFSCREEN_TABS_OFFSCREEN_TABS_API_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs.h"
#include "chrome/browser/extensions/extension_function.h"
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
class TabContents;
namespace content {
class WebContents;
}  // namespace content

// Creates an offscreen tab.
class CreateOffscreenTabFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.create")

  CreateOffscreenTabFunction();

 protected:
  virtual ~CreateOffscreenTabFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateOffscreenTabFunction);
};

// Gets info about an offscreen tab.
class GetOffscreenTabFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.get")

  GetOffscreenTabFunction();

 protected:
  virtual ~GetOffscreenTabFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GetOffscreenTabFunction);
};

// Gets all offscreen tabs created by the tab that invoked this function.
class GetAllOffscreenTabFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.getAll")

  GetAllOffscreenTabFunction();

 protected:
  virtual ~GetAllOffscreenTabFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GetAllOffscreenTabFunction);
};

// Removes an offscreen tab.
class RemoveOffscreenTabFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.remove")

  RemoveOffscreenTabFunction();

 protected:
  virtual ~RemoveOffscreenTabFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoveOffscreenTabFunction);
};

// Synthesizes a keyboard event based on a passed-in JavaScript keyboard event.
// TODO(jstritar): This would be useful on the chrome.tabs API.
class SendKeyboardEventOffscreenTabFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
    "experimental.offscreenTabs.sendKeyboardEvent")

  SendKeyboardEventOffscreenTabFunction();

 protected:
  virtual ~SendKeyboardEventOffscreenTabFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SendKeyboardEventOffscreenTabFunction);
};

// Synthesizes a mouse event based on a passed-in JavaScript mouse event.
// Since only the application knows where the user clicks, x and y coordinates
// need to be passed in as well in this case of click, mousedown, and mouseup.
// TODO(jstritar): This would be useful on the chrome.tabs API.
class SendMouseEventOffscreenTabFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.sendMouseEvent")

  SendMouseEventOffscreenTabFunction();

 protected:
  virtual ~SendMouseEventOffscreenTabFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SendMouseEventOffscreenTabFunction);
};

// Gets a snapshot of the offscreen tab and returns it as a data URL.
class ToDataUrlOffscreenTabFunction : public CaptureVisibleTabFunction {
 public:
  // TODO(jstritar): Rename to toDataUrl.
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.toDataUrl")

  ToDataUrlOffscreenTabFunction();

 protected:
  virtual ~ToDataUrlOffscreenTabFunction();

  // CaptureVisibleTabFunction:
  virtual bool GetTabToCapture(content::WebContents** web_contents) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ToDataUrlOffscreenTabFunction);
};

// Updates an offscreen tab.
class UpdateOffscreenTabFunction : public UpdateTabFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.update")

  UpdateOffscreenTabFunction();

 protected:
  virtual ~UpdateOffscreenTabFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // UpdateTabFunction:
  virtual void PopulateResult() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateOffscreenTabFunction);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_OFFSCREEN_TABS_OFFSCREEN_TABS_API_H_
