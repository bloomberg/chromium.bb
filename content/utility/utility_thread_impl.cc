// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_thread_impl.h"

#include <stddef.h>

#include "base/file_path.h"
#include "base/memory/scoped_vector.h"
#include "content/common/child_process.h"
#include "content/common/child_process_messages.h"
#include "content/common/indexed_db_key.h"
#include "content/common/utility_messages.h"
#include "content/common/webkitplatformsupport_impl.h"
#include "content/public/utility/content_utility_client.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKey.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSerializedScriptValue.h"
#include "webkit/glue/idb_bindings.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

template<typename SRC, typename DEST>
void ConvertVector(const SRC& src, DEST* dest) {
  dest->reserve(src.size());
  for (typename SRC::const_iterator i = src.begin(); i != src.end(); ++i)
    dest->push_back(typename DEST::value_type(*i));
}

}  // namespace

UtilityThreadImpl::UtilityThreadImpl()
    : batch_mode_(false) {
  ChildProcess::current()->AddRefProcess();
  webkit_platform_support_.reset(new content::WebKitPlatformSupportImpl);
  WebKit::initialize(webkit_platform_support_.get());
  content::GetContentClient()->utility()->UtilityThreadStarted();
}

UtilityThreadImpl::~UtilityThreadImpl() {
  WebKit::shutdown();
}

bool UtilityThreadImpl::Send(IPC::Message* msg) {
  return ChildThread::Send(msg);
}

void UtilityThreadImpl::ReleaseProcessIfNeeded() {
  if (!batch_mode_)
    ChildProcess::current()->ReleaseProcess();
}

#if defined(OS_WIN)

void UtilityThreadImpl::PreCacheFont(const LOGFONT& log_font) {
  Send(new ChildProcessHostMsg_PreCacheFont(log_font));
}

void UtilityThreadImpl::ReleaseCachedFonts() {
  Send(new ChildProcessHostMsg_ReleaseCachedFonts());
}

#endif  // OS_WIN


bool UtilityThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  if (content::GetContentClient()->utility()->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(UtilityThreadImpl, msg)
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

void UtilityThreadImpl::OnIDBKeysFromValuesAndKeyPath(
    int id,
    const std::vector<content::SerializedScriptValue>& serialized_script_values,
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

void UtilityThreadImpl::OnInjectIDBKey(
    const IndexedDBKey& key,
    const content::SerializedScriptValue& value,
    const string16& key_path) {
  content::SerializedScriptValue new_value(
      webkit_glue::InjectIDBKey(key, value, key_path));
  Send(new UtilityHostMsg_InjectIDBKey_Finished(new_value));
  ReleaseProcessIfNeeded();
}

void UtilityThreadImpl::OnBatchModeStarted() {
  batch_mode_ = true;
}

void UtilityThreadImpl::OnBatchModeFinished() {
  ChildProcess::current()->ReleaseProcess();
}

#if defined(OS_POSIX)
void UtilityThreadImpl::OnLoadPlugins(
    const std::vector<FilePath>& plugin_paths) {
  webkit::npapi::PluginList* plugin_list =
      webkit::npapi::PluginList::Singleton();

  for (size_t i = 0; i < plugin_paths.size(); ++i) {
    ScopedVector<webkit::npapi::PluginGroup> plugin_groups;
    plugin_list->LoadPlugin(plugin_paths[i], &plugin_groups);

    if (plugin_groups.empty()) {
      Send(new UtilityHostMsg_LoadPluginFailed(i, plugin_paths[i]));
      continue;
    }

    const webkit::npapi::PluginGroup* group = plugin_groups[0];
    DCHECK_EQ(group->web_plugin_infos().size(), 1u);

    Send(new UtilityHostMsg_LoadedPlugin(i, group->web_plugin_infos().front()));
  }

  ReleaseProcessIfNeeded();
}
#endif

