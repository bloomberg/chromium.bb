// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_UI_WRAPPER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_UI_WRAPPER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/callback.h"
#include "chrome/browser/sync/api/syncable_service.h"

class FilePath;
class ExtensionSettings;
class ExtensionSettingsStorage;

// Wrapper for an ExtensionSettings object for dealing with thread ownership.
// This class lives on the UI thread while ExtensionSettings object live on
// the FILE thread.
class ExtensionSettingsUIWrapper {
 public:
  explicit ExtensionSettingsUIWrapper(const FilePath& base_path);

  typedef base::Callback<void(ExtensionSettings*)> SettingsCallback;

  // Runs |callback| on the FILE thread with the extension settings.
  void RunWithSettings(const SettingsCallback& callback);

  ~ExtensionSettingsUIWrapper();

 private:
  // Ref-counted container for the ExtensionSettings object.
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    // Constructed on UI thread.
    Core();

    // Does any FILE thread specific initialization, such as
    // construction of |extension_settings_|.  Must be called before
    // any call to RunWithSettingsOnFileThread().
    void InitOnFileThread(const FilePath& base_path);

    // Runs |callback| with extension settings.
    void RunWithSettingsOnFileThread(const SettingsCallback& callback);

   private:
    // Can be destroyed on either the UI or FILE thread.
    virtual ~Core();
    friend class base::RefCountedThreadSafe<Core>;

    // Lives on the FILE thread.
    ExtensionSettings* extension_settings_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsUIWrapper);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_UI_WRAPPER_H_
