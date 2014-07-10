// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EMBEDDER_MESSAGE_DISPATCHER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EMBEDDER_MESSAGE_DISPATCHER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace base {
class ListValue;
}

/**
 * Dispatcher for messages sent from the DevTools frontend running in an
 * isolated renderer (on chrome-devtools://) to the embedder in the browser.
 *
 * The messages are sent via InspectorFrontendHost.sendMessageToEmbedder method.
 */
class DevToolsEmbedderMessageDispatcher {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void ActivateWindow() = 0;
    virtual void CloseWindow() = 0;
    virtual void SetInspectedPageBounds(const gfx::Rect& rect) = 0;
    virtual void SetContentsResizingStrategy(
        const gfx::Insets& insets, const gfx::Size& min_size) = 0;
    virtual void InspectElementCompleted() = 0;
    virtual void InspectedURLChanged(const std::string& url) = 0;
    virtual void MoveWindow(int x, int y) = 0;
    virtual void SetIsDocked(bool is_docked) = 0;
    virtual void OpenInNewTab(const std::string& url) = 0;
    virtual void SaveToFile(const std::string& url,
                            const std::string& content,
                            bool save_as) = 0;
    virtual void AppendToFile(const std::string& url,
                              const std::string& content) = 0;
    virtual void RequestFileSystems() = 0;
    virtual void AddFileSystem() = 0;
    virtual void RemoveFileSystem(const std::string& file_system_path) = 0;
    virtual void UpgradeDraggedFileSystemPermissions(
        const std::string& file_system_url) = 0;
    virtual void IndexPath(int request_id,
                           const std::string& file_system_path) = 0;
    virtual void StopIndexing(int request_id) = 0;
    virtual void SearchInPath(int request_id,
                              const std::string& file_system_path,
                              const std::string& query) = 0;
    virtual void SetWhitelistedShortcuts(const std::string& message) = 0;
    virtual void ZoomIn() = 0;
    virtual void ZoomOut() = 0;
    virtual void ResetZoom() = 0;
    virtual void OpenUrlOnRemoteDeviceAndInspect(const std::string& browser_id,
                                                 const std::string& url) = 0;

    virtual void SetDeviceCountUpdatesEnabled(bool enabled) = 0;
    virtual void SetDevicesUpdatesEnabled(bool enabled) = 0;
  };

  virtual ~DevToolsEmbedderMessageDispatcher() {}
  virtual bool Dispatch(const std::string& method,
                        const base::ListValue* params,
                        std::string* error) = 0;

  static DevToolsEmbedderMessageDispatcher* createForDevToolsFrontend(
      Delegate* delegate);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EMBEDDER_MESSAGE_DISPATCHER_H_
