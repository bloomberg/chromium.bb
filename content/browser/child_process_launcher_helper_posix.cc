// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper_posix.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "content/browser/file_descriptor_info_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_descriptors.h"
#include "mojo/edk/embedder/platform_handle.h"

namespace content {
namespace internal {

std::unique_ptr<FileDescriptorInfo> CreateDefaultPosixFilesToMap(
    const base::CommandLine& command_line,
    int child_process_id,
    const mojo::edk::PlatformHandle& mojo_client_handle) {
  std::unique_ptr<FileDescriptorInfo> files_to_register(
      FileDescriptorInfoImpl::Create());

  int field_trial_handle = base::FieldTrialList::GetFieldTrialHandle();
  if (field_trial_handle != base::kInvalidPlatformFile)
    files_to_register->Share(kFieldTrialDescriptor, field_trial_handle);

  DCHECK(mojo_client_handle.is_valid());
  files_to_register->Share(kMojoIPCChannel, mojo_client_handle.handle);

  // TODO(jcivelli): remove this "if defined" by making
  // GetAdditionalMappedFilesForChildProcess a no op on Mac.
#if !defined(OS_MACOSX)
  GetContentClient()->browser()->GetAdditionalMappedFilesForChildProcess(
      command_line, child_process_id, files_to_register.get());
#endif

  return files_to_register;
}

}  // namespace internal
}  // namespace content
