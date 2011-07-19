// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_STARTUP_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_STARTUP_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/pack_extension_job.h"

class CommandLine;
class Profile;

// Initialization helpers for various Extension startup actions.
class ExtensionsStartupUtil : public PackExtensionJob::Client {
 public:
  ExtensionsStartupUtil();
  virtual ~ExtensionsStartupUtil();

  virtual void OnPackSuccess(const FilePath& crx_path,
                             const FilePath& output_private_key_path);
  virtual void OnPackFailure(const std::string& error_message);

  // Handle --pack-extension flag from the |cmd_line| by packing the specified
  // extension. Returns false if the pack job failed.
  bool PackExtension(const CommandLine& cmd_line);

  // Handle --uninstall-extension flag from the |cmd_line| by uninstalling the
  // specified extension from |profile|. Returns false if the uninstall job
  // could not be started.
  bool UninstallExtension(const CommandLine& cmd_line, Profile* profile);

 private:
  void ShowPackExtensionMessage(const std::wstring& caption,
                                const std::wstring& message);
  scoped_refptr<PackExtensionJob> pack_job_;
  bool pack_job_succeeded_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsStartupUtil);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_STARTUP_H_
