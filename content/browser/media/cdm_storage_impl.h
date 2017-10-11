// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_STORAGE_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CDM_STORAGE_IMPL_H_

#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_service_base.h"
#include "media/mojo/interfaces/cdm_storage.mojom.h"

namespace storage {
class FileSystemContext;
}

namespace url {
class Origin;
}

namespace content {
class RenderFrameHost;

// This class implements the media::mojom::CdmStorage using the
// PluginPrivateFileSystem for backwards compatibility with CDMs running
// as a pepper plugin.
class CONTENT_EXPORT CdmStorageImpl final
    : public content::FrameServiceBase<media::mojom::CdmStorage> {
 public:
  // The file system is different for each CDM and each origin. So track files
  // in use based on the tuple CDM file system ID, origin, and file name.
  struct FileLockKey {
    FileLockKey(const std::string& cdm_file_system_id,
                const url::Origin& origin,
                const std::string& file_name);
    ~FileLockKey() = default;

    // Allow use as a key in std::set.
    bool operator<(const FileLockKey& other) const;

    std::string cdm_file_system_id;
    url::Origin origin;
    std::string file_name;
  };

  // Check if |cdm_file_system_id| is valid.
  static bool IsValidCdmFileSystemId(const std::string& cdm_file_system_id);

  // Create a CdmStorageImpl object for |cdm_file_system_id| and bind it to
  // |request|.
  static void Create(RenderFrameHost* render_frame_host,
                     const std::string& cdm_file_system_id,
                     media::mojom::CdmStorageRequest request);

  // media::mojom::CdmStorage implementation.
  void Open(const std::string& file_name, OpenCallback callback) final;

 private:
  CdmStorageImpl(RenderFrameHost* render_frame_host,
                 const std::string& cdm_file_system_id,
                 scoped_refptr<storage::FileSystemContext> file_system_context,
                 media::mojom::CdmStorageRequest request);
  ~CdmStorageImpl() final;

  // Called when the file system has been opened (OpenPluginPrivateFileSystem
  // is asynchronous).
  void OnFileSystemOpened(const FileLockKey& key,
                          const std::string& fsid,
                          OpenCallback callback,
                          base::File::Error error);

  // Called when the requested file has been opened.
  void OnFileOpened(const FileLockKey& key,
                    OpenCallback callback,
                    base::File file,
                    const base::Closure& on_close_callback);

  // Files are stored in the PluginPrivateFileSystem, so keep track of the
  // CDM file system ID in order to open the files in the correct context.
  const std::string cdm_file_system_id_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // This is the child process that will actually read and write the file(s)
  // returned, and it needs permission to access the file when it's opened.
  const int child_process_id_;

  // As a lock is taken on a file when Open() is called, it needs to be
  // released if the async operations to open the file haven't completed
  // by the time |this| is destroyed. So keep track of FileLockKey for files
  // that have a lock on them but failed to finish.
  std::set<FileLockKey> pending_open_;

  base::WeakPtrFactory<CdmStorageImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CdmStorageImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_STORAGE_IMPL_H_
