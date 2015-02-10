// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_message_filter.h"

#include "base/memory/singleton.h"
#include "components/crx_file/id_util.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/blob_holder.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "ipc/ipc_message_macros.h"

using content::BrowserThread;
using content::RenderProcessHost;

namespace extensions {

namespace {

class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static ShutdownNotifierFactory* GetInstance() {
    return Singleton<ShutdownNotifierFactory>::get();
  }

 private:
  friend struct DefaultSingletonTraits<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "ExtensionMessageFilter") {
    DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
    DependsOn(ProcessManagerFactory::GetInstance());
  }
  ~ShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(ShutdownNotifierFactory);
};

}  // namespace

ExtensionMessageFilter::ExtensionMessageFilter(int render_process_id,
                                               content::BrowserContext* context)
    : BrowserMessageFilter(ExtensionMsgStart),
      render_process_id_(render_process_id),
      extension_system_(ExtensionSystem::Get(context)),
      process_manager_(ProcessManager::Get(context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  shutdown_notifier_ =
      ShutdownNotifierFactory::GetInstance()->Get(context)->Subscribe(
          base::Bind(&ExtensionMessageFilter::ShutdownOnUIThread,
                     base::Unretained(this)));
}

void ExtensionMessageFilter::EnsureShutdownNotifierFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

ExtensionMessageFilter::~ExtensionMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ExtensionMessageFilter::ShutdownOnUIThread() {
  extension_system_ = nullptr;
  process_manager_ = nullptr;
  shutdown_notifier_.reset();
}

void ExtensionMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
    case ExtensionHostMsg_AddListener::ID:
    case ExtensionHostMsg_RemoveListener::ID:
    case ExtensionHostMsg_AddLazyListener::ID:
    case ExtensionHostMsg_RemoveLazyListener::ID:
    case ExtensionHostMsg_AddFilteredListener::ID:
    case ExtensionHostMsg_RemoveFilteredListener::ID:
    case ExtensionHostMsg_ShouldSuspendAck::ID:
    case ExtensionHostMsg_SuspendAck::ID:
    case ExtensionHostMsg_TransferBlobsAck::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

void ExtensionMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

bool ExtensionMessageFilter::OnMessageReceived(const IPC::Message& message) {
  // If we have been shut down already, return.
  if (!extension_system_)
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionMessageFilter, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddListener,
                        OnExtensionAddListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveListener,
                        OnExtensionRemoveListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddLazyListener,
                        OnExtensionAddLazyListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveLazyListener,
                        OnExtensionRemoveLazyListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddFilteredListener,
                        OnExtensionAddFilteredListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveFilteredListener,
                        OnExtensionRemoveFilteredListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ShouldSuspendAck,
                        OnExtensionShouldSuspendAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_SuspendAck,
                        OnExtensionSuspendAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_TransferBlobsAck,
                        OnExtensionTransferBlobsAck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionMessageFilter::OnExtensionAddListener(
    const std::string& extension_id,
    const GURL& listener_url,
    const std::string& event_name) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  EventRouter* router = extension_system_->event_router();
  if (!router)
    return;

  if (crx_file::id_util::IdIsValid(extension_id)) {
    router->AddEventListener(event_name, process, extension_id);
  } else if (listener_url.is_valid()) {
    router->AddEventListenerForURL(event_name, process, listener_url);
  } else {
    NOTREACHED() << "Tried to add an event listener without a valid "
                 << "extension ID nor listener URL";
  }
}

void ExtensionMessageFilter::OnExtensionRemoveListener(
    const std::string& extension_id,
    const GURL& listener_url,
    const std::string& event_name) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  EventRouter* router = extension_system_->event_router();
  if (!router)
    return;

  if (crx_file::id_util::IdIsValid(extension_id)) {
    router->RemoveEventListener(event_name, process, extension_id);
  } else if (listener_url.is_valid()) {
    router->RemoveEventListenerForURL(event_name, process, listener_url);
  } else {
    NOTREACHED() << "Tried to remove an event listener without a valid "
                 << "extension ID nor listener URL";
  }
}

void ExtensionMessageFilter::OnExtensionAddLazyListener(
    const std::string& extension_id, const std::string& event_name) {
  EventRouter* router = extension_system_->event_router();
  if (!router)
    return;

  router->AddLazyEventListener(event_name, extension_id);
}

void ExtensionMessageFilter::OnExtensionRemoveLazyListener(
    const std::string& extension_id, const std::string& event_name) {
  EventRouter* router = extension_system_->event_router();
  if (!router)
    return;

  router->RemoveLazyEventListener(event_name, extension_id);
}

void ExtensionMessageFilter::OnExtensionAddFilteredListener(
    const std::string& extension_id,
    const std::string& event_name,
    const base::DictionaryValue& filter,
    bool lazy) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  EventRouter* router = extension_system_->event_router();
  if (!router)
    return;

  router->AddFilteredEventListener(
      event_name, process, extension_id, filter, lazy);
}

void ExtensionMessageFilter::OnExtensionRemoveFilteredListener(
    const std::string& extension_id,
    const std::string& event_name,
    const base::DictionaryValue& filter,
    bool lazy) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  EventRouter* router = extension_system_->event_router();
  if (!router)
    return;

  router->RemoveFilteredEventListener(
      event_name, process, extension_id, filter, lazy);
}

void ExtensionMessageFilter::OnExtensionShouldSuspendAck(
     const std::string& extension_id, int sequence_id) {
  process_manager_->OnShouldSuspendAck(extension_id, sequence_id);
}

void ExtensionMessageFilter::OnExtensionSuspendAck(
     const std::string& extension_id) {
  process_manager_->OnSuspendAck(extension_id);
}

void ExtensionMessageFilter::OnExtensionTransferBlobsAck(
    const std::vector<std::string>& blob_uuids) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;

  BlobHolder::FromRenderProcessHost(process)->DropBlobs(blob_uuids);
}

}  // namespace extensions
