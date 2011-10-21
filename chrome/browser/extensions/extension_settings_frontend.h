// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_FRONTEND_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_FRONTEND_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/callback.h"
#include "chrome/browser/sync/api/syncable_service.h"

class FilePath;
class ExtensionSettingsBackend;
class ExtensionSettingsStorage;

// The component of extension settings which runs on the UI thread, as opposed
// to ExtensionSettingsBackend which lives on the FILE thread.
class ExtensionSettingsFrontend {
 public:
  explicit ExtensionSettingsFrontend(const FilePath& base_path);

  typedef base::Callback<void(ExtensionSettingsBackend*)> BackendCallback;

  // Runs |callback| on the FILE thread with the extension settings.
  void RunWithBackend(const BackendCallback& callback);

  ~ExtensionSettingsFrontend();

 private:
  // Ref-counted container for the ExtensionSettingsBackend object.
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    // Constructed on UI thread.
    Core();

    // Does any FILE thread specific initialization, such as construction of
    // |backend_|.  Must be called before any call to
    // RunWithBackendOnFileThread().
    void InitOnFileThread(const FilePath& base_path);

    // Runs |callback| with the extension backend.
    void RunWithBackendOnFileThread(const BackendCallback& callback);

   private:
    // Can be destroyed on either the UI or FILE thread.
    virtual ~Core();
    friend class base::RefCountedThreadSafe<Core>;

    // Lives on the FILE thread.
    ExtensionSettingsBackend* backend_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsFrontend);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_FRONTEND_H_
