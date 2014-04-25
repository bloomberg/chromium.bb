// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_WEBSTORE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_WEBSTORE_API_H_

#include <list>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/common/extensions/api/webstore/webstore_api_constants.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

namespace base {
class ListValue;
}

namespace content {
class BrowserContext;
}

namespace IPC {
class Sender;
}

namespace extensions {
class InstallTracker;

class WebstoreAPI : public BrowserContextKeyedAPI,
                    public InstallObserver {
 public:
  explicit WebstoreAPI(content::BrowserContext* browser_context);
  virtual ~WebstoreAPI();

  static WebstoreAPI* Get(content::BrowserContext* browser_context);

  // Called whenever an inline extension install is started. Examines
  // |listener_mask| to determine if a download progress or install
  // stage listener should be added.
  // |routing_id| refers to the id to which we send any return messages;
  // |ipc_sender| is the sender through which we send them (typically this
  // is the TabHelper which started the inline install).
  void OnInlineInstallStart(int routing_id,
                            IPC::Sender* ipc_sender,
                            const std::string& extension_id,
                            int listener_mask);

  // Called when an inline extension install finishes. Removes any listeners
  // related to the |routing_id|-|extension_id| pair.
  void OnInlineInstallFinished(int routing_id, const std::string& extension_id);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<WebstoreAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<WebstoreAPI>;

  // A simple struct to hold our listeners' information for each observed
  // install.
  struct ObservedInstallInfo;
  typedef std::list<ObservedInstallInfo> ObservedInstallInfoList;

  // Sends an installation stage update message if we are observing
  // the extension's install.
  void SendInstallMessageIfObserved(const std::string& extension_id,
                                    api::webstore::InstallStage install_stage);

  // Removes listeners for the given |extension_id|-|routing_id| pair from
  // |listeners|.
  void RemoveListeners(int routing_id,
                       const std::string& extension_id,
                       ObservedInstallInfoList* listeners);

  // InstallObserver implementation.
  virtual void OnBeginExtensionDownload(const std::string& extension_id)
      OVERRIDE;
  virtual void OnDownloadProgress(const std::string& extension_id,
                                  int percent_downloaded) OVERRIDE;
  virtual void OnBeginCrxInstall(const std::string& extension_id) OVERRIDE;
  virtual void OnShutdown() OVERRIDE;

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "WebstoreAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;

  ObservedInstallInfoList download_progress_listeners_;
  ObservedInstallInfoList install_stage_listeners_;
  content::BrowserContext* browser_context_;
  scoped_ptr<ScopedObserver<InstallTracker, InstallObserver> >
      install_observer_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBSTORE_WEBSTORE_API_H_
