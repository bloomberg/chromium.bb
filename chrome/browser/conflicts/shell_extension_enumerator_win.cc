// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/shell_extension_enumerator_win.h"

#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/registry.h"
#include "chrome/browser/conflicts/module_info_util_win.h"

namespace {

void ReadShellExtensions(
    HKEY parent,
    const base::Callback<void(const base::FilePath&)>& callback,
    int* nb_shell_extensions) {
  for (base::win::RegistryValueIterator iter(
           parent, ShellExtensionEnumerator::kShellExtensionRegistryKey);
       iter.Valid(); ++iter) {
    base::string16 key = base::StringPrintf(
        ShellExtensionEnumerator::kClassIdRegistryKeyFormat, iter.Name());

    base::win::RegKey clsid;
    if (clsid.Open(HKEY_CLASSES_ROOT, key.c_str(), KEY_READ) != ERROR_SUCCESS)
      continue;

    base::string16 dll;
    if (clsid.ReadValue(L"", &dll) != ERROR_SUCCESS)
      continue;

    nb_shell_extensions++;
    callback.Run(base::FilePath(dll));
  }
}

}  // namespace

// static
const wchar_t ShellExtensionEnumerator::kShellExtensionRegistryKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell "
    L"Extensions\\Approved";

// static
const wchar_t ShellExtensionEnumerator::kClassIdRegistryKeyFormat[] =
    L"CLSID\\%ls\\InProcServer32";

ShellExtensionEnumerator::ShellExtensionEnumerator(
    const OnShellExtensionEnumeratedCallback&
        on_shell_extension_enumerated_callback)
    : on_shell_extension_enumerated_callback_(
          on_shell_extension_enumerated_callback),
      weak_ptr_factory_(this) {
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::Bind(
          &EnumerateShellExtensionsImpl, base::SequencedTaskRunnerHandle::Get(),
          base::Bind(&ShellExtensionEnumerator::OnShellExtensionEnumerated,
                     weak_ptr_factory_.GetWeakPtr())));
}

ShellExtensionEnumerator::~ShellExtensionEnumerator() = default;

// static
void ShellExtensionEnumerator::EnumerateShellExtensionPaths(
    const base::Callback<void(const base::FilePath&)>& callback) {
  base::ThreadRestrictions::AssertIOAllowed();

  int nb_shell_extensions = 0;
  ReadShellExtensions(HKEY_LOCAL_MACHINE, callback, &nb_shell_extensions);
  ReadShellExtensions(HKEY_CURRENT_USER, callback, &nb_shell_extensions);

  base::UmaHistogramCounts100("ThirdPartyModules.ShellExtensionsCount",
                              nb_shell_extensions);
}

void ShellExtensionEnumerator::OnShellExtensionEnumerated(
    const base::FilePath& path,
    uint32_t size_of_image,
    uint32_t time_date_stamp) {
  on_shell_extension_enumerated_callback_.Run(path, size_of_image,
                                              time_date_stamp);
}

// static
void ShellExtensionEnumerator::EnumerateShellExtensionsImpl(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const OnShellExtensionEnumeratedCallback& callback) {
  ShellExtensionEnumerator::EnumerateShellExtensionPaths(
      base::Bind(&ShellExtensionEnumerator::OnShellExtensionPathEnumerated,
                 task_runner, callback));
}

// static
void ShellExtensionEnumerator::OnShellExtensionPathEnumerated(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const OnShellExtensionEnumeratedCallback& callback,
    const base::FilePath& path) {
  uint32_t size_of_image = 0;
  uint32_t time_date_stamp = 0;
  if (!GetModuleImageSizeAndTimeDateStamp(path, &size_of_image,
                                          &time_date_stamp)) {
    return;
  }

  task_runner->PostTask(
      FROM_HERE, base::Bind(callback, path, size_of_image, time_date_stamp));
}
