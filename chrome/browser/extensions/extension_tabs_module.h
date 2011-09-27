// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_H__
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class BackingStore;
class SkBitmap;

// Windows
class GetWindowFunction : public SyncExtensionFunction {
  virtual ~GetWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.get")
};
class GetCurrentWindowFunction : public SyncExtensionFunction {
  virtual ~GetCurrentWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.getCurrent")
};
class GetLastFocusedWindowFunction : public SyncExtensionFunction {
  virtual ~GetLastFocusedWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.getLastFocused")
};
class GetAllWindowsFunction : public SyncExtensionFunction {
  virtual ~GetAllWindowsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.getAll")
};
class CreateWindowFunction : public SyncExtensionFunction {
  virtual ~CreateWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.create")
};
class UpdateWindowFunction : public SyncExtensionFunction {
  virtual ~UpdateWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.update")
};
class RemoveWindowFunction : public SyncExtensionFunction {
  virtual ~RemoveWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("windows.remove")
};

// Tabs
class GetTabFunction : public SyncExtensionFunction {
  virtual ~GetTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.get")
};
class GetCurrentTabFunction : public SyncExtensionFunction {
  virtual ~GetCurrentTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.getCurrent")
};
class GetSelectedTabFunction : public SyncExtensionFunction {
  virtual ~GetSelectedTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.getSelected")
};
class GetAllTabsInWindowFunction : public SyncExtensionFunction {
  virtual ~GetAllTabsInWindowFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.getAllInWindow")
};
class CreateTabFunction : public SyncExtensionFunction {
  virtual ~CreateTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.create")
};
class UpdateTabFunction : public AsyncExtensionFunction,
                          public TabContentsObserver {
 public:
  UpdateTabFunction();
 private:
  virtual ~UpdateTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message);
  void OnExecuteCodeFinished(int request_id,
                             bool success,
                             const std::string& error);
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.update")
};
class MoveTabFunction : public SyncExtensionFunction {
  virtual ~MoveTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.move")
};
class ReloadTabFunction : public SyncExtensionFunction {
  virtual ~ReloadTabFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.reload")
};
class RemoveTabFunction : public SyncExtensionFunction {
  virtual ~RemoveTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.remove")
};
class DetectTabLanguageFunction : public AsyncExtensionFunction,
                                  public NotificationObserver {
 private:
  virtual ~DetectTabLanguageFunction() {}
  virtual bool RunImpl() OVERRIDE;

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;
  void GotLanguage(const std::string& language);
  NotificationRegistrar registrar_;
  DECLARE_EXTENSION_FUNCTION_NAME("tabs.detectLanguage")
};
class CaptureVisibleTabFunction : public AsyncExtensionFunction,
                                  public NotificationObserver {
 private:
  enum ImageFormat {
    FORMAT_JPEG,
    FORMAT_PNG
  };

  // The default quality setting used when encoding jpegs.
  static const int kDefaultQuality;

  virtual ~CaptureVisibleTabFunction() {}
  virtual bool RunImpl() OVERRIDE;
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

  DECLARE_EXTENSION_FUNCTION_NAME("tabs.captureVisibleTab")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TABS_MODULE_H__
