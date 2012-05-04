// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
#define CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/child_thread.h"
#include "content/common/content_export.h"
#include "content/public/utility/utility_thread.h"

class FilePath;

namespace content {
class IndexedDBKey;
class IndexedDBKeyPath;
class SerializedScriptValue;
class WebKitPlatformSupportImpl;
}

// This class represents the background thread where the utility task runs.
class UtilityThreadImpl : public content::UtilityThread,
                          public ChildThread {
 public:
  UtilityThreadImpl();
  virtual ~UtilityThreadImpl();

  virtual bool Send(IPC::Message* msg) OVERRIDE;
  virtual void ReleaseProcessIfNeeded() OVERRIDE;
#if defined(OS_WIN)
  virtual void PreCacheFont(const LOGFONT& log_font) OVERRIDE;
  virtual void ReleaseCachedFonts() OVERRIDE;
#endif

 private:
  // ChildThread implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& msg) OVERRIDE;

  // IPC message handlers.
  void OnIDBKeysFromValuesAndKeyPath(
      int id,
      const std::vector<content::SerializedScriptValue>&
          serialized_script_values,
      const content::IndexedDBKeyPath& idb_key_path);
  void OnInjectIDBKey(const content::IndexedDBKey& key,
                      const content::SerializedScriptValue& value,
                      const content::IndexedDBKeyPath& key_path);
  void OnBatchModeStarted();
  void OnBatchModeFinished();

#if defined(OS_POSIX)
  void OnLoadPlugins(const std::vector<FilePath>& plugin_paths);
#endif  // OS_POSIX

  // True when we're running in batch mode.
  bool batch_mode_;

  scoped_ptr<content::WebKitPlatformSupportImpl> webkit_platform_support_;

  DISALLOW_COPY_AND_ASSIGN(UtilityThreadImpl);
};

#endif  // CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
