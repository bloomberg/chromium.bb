// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_THREAD_H_
#define CONTENT_UTILITY_UTILITY_THREAD_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "content/common/child_thread.h"
#include "content/common/content_export.h"

class FilePath;
class IndexedDBKey;
class SerializedScriptValue;

namespace webkit {
struct WebPluginInfo;
}

namespace webkit_glue {
class WebKitPlatformSupportImpl;
}

// This class represents the background thread where the utility task runs.
class UtilityThread : public ChildThread {
 public:
  UtilityThread();
  virtual ~UtilityThread();

  // Releases the process if we are not (or no longer) in batch mode.
  CONTENT_EXPORT void ReleaseProcessIfNeeded();

  // Returns the one utility thread.
  static UtilityThread* current() {
    return static_cast<UtilityThread*>(ChildThread::current());
  }

 private:
  // ChildThread implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& msg) OVERRIDE;

  // IPC message handlers.
  void OnIDBKeysFromValuesAndKeyPath(
      int id,
      const std::vector<SerializedScriptValue>& serialized_script_values,
      const string16& idb_key_path);
  void OnInjectIDBKey(const IndexedDBKey& key,
                      const SerializedScriptValue& value,
                      const string16& key_path);
  void OnBatchModeStarted();
  void OnBatchModeFinished();

#if defined(OS_POSIX)
  void OnLoadPlugins(
      const std::vector<FilePath>& extra_plugin_paths,
      const std::vector<FilePath>& extra_plugin_dirs,
      const std::vector<webkit::WebPluginInfo>& internal_plugins);
#endif  // OS_POSIX

  // True when we're running in batch mode.
  bool batch_mode_;

  scoped_ptr<webkit_glue::WebKitPlatformSupportImpl> webkit_platform_support_;

  DISALLOW_COPY_AND_ASSIGN(UtilityThread);
};

#endif  // CONTENT_UTILITY_UTILITY_THREAD_H_
