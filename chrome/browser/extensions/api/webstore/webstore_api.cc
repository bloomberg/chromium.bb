// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore/webstore_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "extensions/browser/extension_system.h"
#include "ipc/ipc_sender.h"

namespace extensions {

namespace {

base::LazyInstance<BrowserContextKeyedAPIFactory<WebstoreAPI> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

struct WebstoreAPI::ObservedInstallInfo {
  ObservedInstallInfo(int routing_id,
                      const std::string& extension_id,
                      IPC::Sender* ipc_sender);
  ~ObservedInstallInfo();

  int routing_id;
  std::string extension_id;
  IPC::Sender* ipc_sender;
};

WebstoreAPI::ObservedInstallInfo::ObservedInstallInfo(
    int routing_id,
    const std::string& extension_id,
    IPC::Sender* ipc_sender)
    : routing_id(routing_id),
      extension_id(extension_id),
      ipc_sender(ipc_sender) {}

WebstoreAPI::ObservedInstallInfo::~ObservedInstallInfo() {}

WebstoreAPI::WebstoreAPI(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      install_observer_(
          new ScopedObserver<InstallTracker, InstallObserver>(this)) {
  install_observer_->Add(InstallTrackerFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context)));
}

WebstoreAPI::~WebstoreAPI() {}

// static
WebstoreAPI* WebstoreAPI::Get(content::BrowserContext* browser_context) {
  return BrowserContextKeyedAPIFactory<WebstoreAPI>::Get(browser_context);
}

void WebstoreAPI::OnInlineInstallStart(int routing_id,
                                       IPC::Sender* ipc_sender,
                                       const std::string& extension_id,
                                       int listeners_mask) {
  if (listeners_mask & api::webstore::INSTALL_STAGE_LISTENER) {
    install_stage_listeners_.push_back(
        ObservedInstallInfo(routing_id, extension_id, ipc_sender));
  }

  if (listeners_mask & api::webstore::DOWNLOAD_PROGRESS_LISTENER) {
    download_progress_listeners_.push_back(
        ObservedInstallInfo(routing_id, extension_id, ipc_sender));
  }
}

void WebstoreAPI::OnInlineInstallFinished(int routing_id,
                                          const std::string& extension_id) {
  RemoveListeners(routing_id, extension_id, &download_progress_listeners_);
  RemoveListeners(routing_id, extension_id, &install_stage_listeners_);
}

void WebstoreAPI::OnBeginExtensionDownload(const std::string& extension_id) {
  SendInstallMessageIfObserved(extension_id,
                               api::webstore::INSTALL_STAGE_DOWNLOADING);
}

void WebstoreAPI::OnDownloadProgress(const std::string& extension_id,
                                     int percent_downloaded) {
  for (ObservedInstallInfoList::const_iterator iter =
           download_progress_listeners_.begin();
       iter != download_progress_listeners_.end();
       ++iter) {
    if (iter->extension_id == extension_id) {
      iter->ipc_sender->Send(new ExtensionMsg_InlineInstallDownloadProgress(
          iter->routing_id, percent_downloaded));
    }
  }
}

void WebstoreAPI::OnBeginCrxInstall(const std::string& extension_id) {
  SendInstallMessageIfObserved(extension_id,
                               api::webstore::INSTALL_STAGE_INSTALLING);
}

void WebstoreAPI::OnShutdown() {
  install_observer_.reset();
}

void WebstoreAPI::Shutdown() {}

// static
BrowserContextKeyedAPIFactory<WebstoreAPI>* WebstoreAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void WebstoreAPI::SendInstallMessageIfObserved(
    const std::string& extension_id,
    api::webstore::InstallStage install_stage) {
  for (ObservedInstallInfoList::const_iterator iter =
           install_stage_listeners_.begin();
       iter != install_stage_listeners_.end();
       ++iter) {
    if (iter->extension_id == extension_id) {
      iter->ipc_sender->Send(new ExtensionMsg_InlineInstallStageChanged(
          iter->routing_id, install_stage));
    }
  }
}

void WebstoreAPI::RemoveListeners(int routing_id,
                                  const std::string& extension_id,
                                  ObservedInstallInfoList* listeners) {
  for (ObservedInstallInfoList::iterator iter = listeners->begin();
       iter != listeners->end();) {
    if (iter->extension_id == extension_id && iter->routing_id == routing_id)
      iter = listeners->erase(iter);
    else
      ++iter;
  }
}

}  // namespace extensions
