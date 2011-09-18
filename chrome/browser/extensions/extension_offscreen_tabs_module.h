// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_OFFSCREEN_TABS_MODULE_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_OFFSCREEN_TABS_MODULE_H__
#pragma once

#include <string>

#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class BackingStore;
class SkBitmap;
class TabContents;
class TabContentsWrapper;

namespace base {
class DictionaryValue;
class ListValue;
}

// Creates an offscreen tab and adds it to a map of tabs and their child
// offscreen tabs.
class CreateOffscreenTabFunction : public SyncExtensionFunction {
 public:
  CreateOffscreenTabFunction();
  virtual ~CreateOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.create")
  DISALLOW_COPY_AND_ASSIGN(CreateOffscreenTabFunction);
};

// Gets info about an offscreen tab.
class GetOffscreenTabFunction : public SyncExtensionFunction {
 public:
  GetOffscreenTabFunction();
  virtual ~GetOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.get")
  DISALLOW_COPY_AND_ASSIGN(GetOffscreenTabFunction);
};

// Gets all offscreen tabs created by the tab that invoked this function.
class GetAllOffscreenTabFunction : public SyncExtensionFunction {
 public:
  GetAllOffscreenTabFunction();
  virtual ~GetAllOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.getAll")
  DISALLOW_COPY_AND_ASSIGN(GetAllOffscreenTabFunction);
};

// Removes an offscreen tab.
class RemoveOffscreenTabFunction : public SyncExtensionFunction {
 public:
  RemoveOffscreenTabFunction();
  virtual ~RemoveOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.remove")
  DISALLOW_COPY_AND_ASSIGN(RemoveOffscreenTabFunction);
};

// Synthesizes a keyboard event based on a passed-in JavaScript keyboard event.
class SendKeyboardEventOffscreenTabFunction : public SyncExtensionFunction {
 public:
  SendKeyboardEventOffscreenTabFunction();
  virtual ~SendKeyboardEventOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME(
    "experimental.offscreenTabs.sendKeyboardEvent")
  DISALLOW_COPY_AND_ASSIGN(SendKeyboardEventOffscreenTabFunction);
};

// Synthesizes a mouse event based on a passed-in JavaScript mouse event.
// Since only the application knows where the user clicks, x and y coordinates
// need to be passed in as well in this case of click, mousedown, and mouseup.
class SendMouseEventOffscreenTabFunction : public SyncExtensionFunction {
 public:
  SendMouseEventOffscreenTabFunction();
  virtual ~SendMouseEventOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.sendMouseEvent")
  DISALLOW_COPY_AND_ASSIGN(SendMouseEventOffscreenTabFunction);
};

// Gets a snapshot of the offscreen tabs and returns it as a data URL.
class ToDataUrlOffscreenTabFunction : public AsyncExtensionFunction,
                                      public NotificationObserver {
 public:
  ToDataUrlOffscreenTabFunction();
  virtual ~ToDataUrlOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  enum ImageFormat {
    FORMAT_JPEG,
    FORMAT_PNG
  };

  // The default quality setting used when encoding jpegs.
  static const int kDefaultQuality = 90;
  virtual bool CaptureSnapshotFromBackingStore(BackingStore* backing_store);
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;
  virtual void SendResultFromBitmap(const SkBitmap& screen_capture);

  NotificationRegistrar registrar_;

  // The format (JPEG vs PNG) of the resulting image.  Set in RunImpl().
  ImageFormat image_format_;

  // Quality setting to use when encoding jpegs.  Set in RunImpl().
  int image_quality_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.toDataUrl")
  DISALLOW_COPY_AND_ASSIGN(ToDataUrlOffscreenTabFunction);
};

// Updates an offscreen tab.
class UpdateOffscreenTabFunction : public AsyncExtensionFunction,
                                   public TabContentsObserver {
 public:
  UpdateOffscreenTabFunction();
  virtual ~UpdateOffscreenTabFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  virtual bool OnMessageReceived(const IPC::Message& message);
  void OnExecuteCodeFinished(int request_id,
                             bool success,
                             const std::string& error);

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.offscreenTabs.update")
  DISALLOW_COPY_AND_ASSIGN(UpdateOffscreenTabFunction);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_OFFSCREEN_TABS_MODULE_H__

