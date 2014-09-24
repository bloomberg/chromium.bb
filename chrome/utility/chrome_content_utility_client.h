// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
#define CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "content/public/utility/content_utility_client.h"
#include "ipc/ipc_platform_file.h"

namespace base {
class FilePath;
struct FileDescriptor;
}

class UtilityMessageHandler;

class ChromeContentUtilityClient : public content::ContentUtilityClient {
 public:
  ChromeContentUtilityClient();
  virtual ~ChromeContentUtilityClient();

  virtual void UtilityThreadStarted() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  static void PreSandboxStartup();

  // Shared with extensions::ExtensionsHandler.
  static void DecodeImage(const std::vector<unsigned char>& encoded_data);

 private:
  // IPC message handlers.
  void OnUnpackWebResource(const std::string& resource_data);
  void OnDecodeImage(const std::vector<unsigned char>& encoded_data);
  void OnRobustJPEGDecodeImage(
      const std::vector<unsigned char>& encoded_data);

#if defined(OS_CHROMEOS)
  void OnCreateZipFile(const base::FilePath& src_dir,
                       const std::vector<base::FilePath>& src_relative_paths,
                       const base::FileDescriptor& dest_fd);
#endif  // defined(OS_CHROMEOS)

  void OnPatchFileBsdiff(const base::FilePath& input_file,
                         const base::FilePath& patch_file,
                         const base::FilePath& output_file);
  void OnPatchFileCourgette(const base::FilePath& input_file,
                            const base::FilePath& patch_file,
                            const base::FilePath& output_file);
  void OnStartupPing();
#if defined(FULL_SAFE_BROWSING)
  void OnAnalyzeZipFileForDownloadProtection(
      const IPC::PlatformFileForTransit& zip_file);
#endif
#if defined(ENABLE_EXTENSIONS)
  void OnParseMediaMetadata(const std::string& mime_type,
                            int64 total_size,
                            bool get_attached_images);
#endif

  typedef ScopedVector<UtilityMessageHandler> Handlers;
  Handlers handlers_;

  // Flag to enable whitelisting.
  bool filter_messages_;
  // A list of message_ids to filter.
  std::set<int> message_id_whitelist_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentUtilityClient);
};

#endif  // CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
