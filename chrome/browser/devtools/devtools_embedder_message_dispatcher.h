// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EMBEDDER_MESSAGE_DISPATCHER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EMBEDDER_MESSAGE_DISPATCHER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

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

    virtual void ActivateWindow(int request_id) = 0;
    virtual void CloseWindow(int request_id) = 0;
    virtual void LoadCompleted(int request_id) = 0;
    virtual void SetInspectedPageBounds(int request_id,
                                        const gfx::Rect& rect) = 0;
    virtual void InspectElementCompleted(int request_id) = 0;
    virtual void InspectedURLChanged(int request_id,
                                     const std::string& url) = 0;
    virtual void SetIsDocked(int request_id, bool is_docked) = 0;
    virtual void OpenInNewTab(int request_id, const std::string& url) = 0;
    virtual void SaveToFile(int request_id,
                            const std::string& url,
                            const std::string& content,
                            bool save_as) = 0;
    virtual void AppendToFile(int request_id,
                              const std::string& url,
                              const std::string& content) = 0;
    virtual void RequestFileSystems(int request_id) = 0;
    virtual void AddFileSystem(int request_id) = 0;
    virtual void RemoveFileSystem(int request_id,
                                  const std::string& file_system_path) = 0;
    virtual void UpgradeDraggedFileSystemPermissions(
        int request_id,
        const std::string& file_system_url) = 0;
    virtual void IndexPath(int request_id,
                           int index_request_id,
                           const std::string& file_system_path) = 0;
    virtual void StopIndexing(int request_id, int index_request_id) = 0;
    virtual void LoadNetworkResource(int request_id,
                                     const std::string& url,
                                     const std::string& headers,
                                     int stream_id) = 0;
    virtual void SearchInPath(int request_id,
                              int search_request_id,
                              const std::string& file_system_path,
                              const std::string& query) = 0;
    virtual void SetWhitelistedShortcuts(int request_id,
                                         const std::string& message) = 0;
    virtual void ZoomIn(int request_id) = 0;
    virtual void ZoomOut(int request_id) = 0;
    virtual void ResetZoom(int request_id) = 0;
    virtual void OpenUrlOnRemoteDeviceAndInspect(int request_id,
                                                 const std::string& browser_id,
                                                 const std::string& url) = 0;
    virtual void SetDeviceCountUpdatesEnabled(int request_id, bool enabled) = 0;
    virtual void SetDevicesUpdatesEnabled(int request_id, bool enabled) = 0;
    virtual void SendMessageToBrowser(int request_id,
                                      const std::string& message) = 0;
    virtual void RecordActionUMA(int request_id,
                                 const std::string& name,
                                 int action) = 0;
  };

  virtual ~DevToolsEmbedderMessageDispatcher() {}
  virtual bool Dispatch(int request_id,
                        const std::string& method,
                        const base::ListValue* params) = 0;

  static DevToolsEmbedderMessageDispatcher* CreateForDevToolsFrontend(
      Delegate* delegate);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_EMBEDDER_MESSAGE_DISPATCHER_H_
