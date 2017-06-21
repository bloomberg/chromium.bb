// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_SHELL_EXTENSION_ENUMERATOR_WIN_H_
#define CHROME_BROWSER_CONFLICTS_SHELL_EXTENSION_ENUMERATOR_WIN_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SequencedTaskRunner;
}

// Finds shell extensions installed on the computer by enumerating the registry.
class ShellExtensionEnumerator {
 public:
  // The path to the registry key where shell extensions are registered.
  static const wchar_t kShellExtensionRegistryKey[];
  static const wchar_t kClassIdRegistryKeyFormat[];

  using OnShellExtensionEnumeratedCallback =
      base::Callback<void(const base::FilePath&, uint32_t, uint32_t)>;

  explicit ShellExtensionEnumerator(const OnShellExtensionEnumeratedCallback&
                                        on_shell_extension_enumerated_callback);
  ~ShellExtensionEnumerator();

  // Enumerates registered shell extensions, and invokes |callback| once per
  // shell extension found. Must be called on a blocking sequence.
  //
  // TODO(pmonette): Move this function to protected when
  //                 enumerate_modules_model.cc gets deleted.
  static void EnumerateShellExtensionPaths(
      const base::Callback<void(const base::FilePath&)>& callback);

 private:
  void OnShellExtensionEnumerated(const base::FilePath& path,
                                  uint32_t size_of_image,
                                  uint32_t time_date_stamp);

  static void EnumerateShellExtensionsImpl(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const OnShellExtensionEnumeratedCallback& callback);

  static void OnShellExtensionPathEnumerated(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const OnShellExtensionEnumeratedCallback& callback,
      const base::FilePath& path);

  OnShellExtensionEnumeratedCallback on_shell_extension_enumerated_callback_;

  base::WeakPtrFactory<ShellExtensionEnumerator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionEnumerator);
};

#endif  // CHROME_BROWSER_CONFLICTS_SHELL_EXTENSION_ENUMERATOR_WIN_H_
