// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_thread.h"

#include <stddef.h>

#include "base/file_path.h"
#include "content/common/child_process.h"
#include "content/common/indexed_db_key.h"
#include "content/common/utility_messages.h"
#include "content/utility/content_utility_client.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKey.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "webkit/glue/idb_bindings.h"
#include "webkit/glue/webkitplatformsupport_impl.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

template<typename SRC, typename DEST>
void ConvertVector(const SRC& src, DEST* dest) {
  dest->reserve(src.size());
  for (typename SRC::const_iterator i = src.begin(); i != src.end(); ++i)
    dest->push_back(typename DEST::value_type(*i));
}

}  // namespace

UtilityThread::UtilityThread()
    : batch_mode_(false) {
  ChildProcess::current()->AddRefProcess();
  webkit_platform_support_.reset(new webkit_glue::WebKitPlatformSupportImpl);
  WebKit::initialize(webkit_platform_support_.get());
  content::GetContentClient()->utility()->UtilityThreadStarted();
}

UtilityThread::~UtilityThread() {
  WebKit::shutdown();
}

bool UtilityThread::OnControlMessageReceived(const IPC::Message& msg) {
  if (content::GetContentClient()->utility()->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(UtilityThread, msg)
    IPC_MESSAGE_HANDLER(UtilityMsg_IDBKeysFromValuesAndKeyPath,
                        OnIDBKeysFromValuesAndKeyPath)
    IPC_MESSAGE_HANDLER(UtilityMsg_InjectIDBKey, OnInjectIDBKey)
    IPC_MESSAGE_HANDLER(UtilityMsg_BatchMode_Started, OnBatchModeStarted)
    IPC_MESSAGE_HANDLER(UtilityMsg_BatchMode_Finished, OnBatchModeFinished)
#if defined(OS_POSIX)
    IPC_MESSAGE_HANDLER(UtilityMsg_LoadPlugins, OnLoadPlugins)
#endif  // OS_POSIX
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void UtilityThread::OnIDBKeysFromValuesAndKeyPath(
    int id,
    const std::vector<SerializedScriptValue>& serialized_script_values,
    const string16& idb_key_path) {
  std::vector<WebKit::WebSerializedScriptValue> web_values;
  ConvertVector(serialized_script_values, &web_values);
  std::vector<WebKit::WebIDBKey> web_keys;
  bool error = webkit_glue::IDBKeysFromValuesAndKeyPath(
      web_values, idb_key_path, &web_keys);
  if (error) {
    Send(new UtilityHostMsg_IDBKeysFromValuesAndKeyPath_Failed(id));
    return;
  }
  std::vector<IndexedDBKey> keys;
  ConvertVector(web_keys, &keys);
  Send(new UtilityHostMsg_IDBKeysFromValuesAndKeyPath_Succeeded(id, keys));
  ReleaseProcessIfNeeded();
}

void UtilityThread::OnInjectIDBKey(const IndexedDBKey& key,
                                   const SerializedScriptValue& value,
                                   const string16& key_path) {
  SerializedScriptValue new_value(webkit_glue::InjectIDBKey(key, value,
                                                              key_path));
  Send(new UtilityHostMsg_InjectIDBKey_Finished(new_value));
  ReleaseProcessIfNeeded();
}

void UtilityThread::OnBatchModeStarted() {
  batch_mode_ = true;
}

void UtilityThread::OnBatchModeFinished() {
  ChildProcess::current()->ReleaseProcess();
}

#if defined(OS_POSIX)
void UtilityThread::OnLoadPlugins(
    const std::vector<FilePath>& extra_plugin_paths,
    const std::vector<FilePath>& extra_plugin_dirs,
    const std::vector<webkit::WebPluginInfo>& internal_plugins) {
  webkit::npapi::PluginList* plugin_list =
      webkit::npapi::PluginList::Singleton();

  // Create the PluginList and set the paths from which to load plugins. Iterate
  // in reverse to preserve the order when pushing back.
  std::vector<FilePath>::const_reverse_iterator it;
  for (it = extra_plugin_paths.rbegin();
       it != extra_plugin_paths.rend();
       ++it) {
    plugin_list->AddExtraPluginPath(*it);
  }
  for (it = extra_plugin_dirs.rbegin(); it != extra_plugin_dirs.rend(); ++it) {
    plugin_list->AddExtraPluginDir(*it);
  }
  for (std::vector<webkit::WebPluginInfo>::const_reverse_iterator it =
           internal_plugins.rbegin();
       it != internal_plugins.rend();
       ++it) {
    plugin_list->RegisterInternalPlugin(*it);
  }

  std::vector<webkit::WebPluginInfo> plugins;
  plugin_list->GetPlugins(&plugins);

  Send(new UtilityHostMsg_LoadedPlugins(plugins));
  UtilityThread::current()->ReleaseProcessIfNeeded();
}
#endif

void UtilityThread::ReleaseProcessIfNeeded() {
  if (!batch_mode_)
    ChildProcess::current()->ReleaseProcess();
}
