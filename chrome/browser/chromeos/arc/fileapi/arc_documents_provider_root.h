// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/arc/common/file_system.mojom.h"
#include "storage/browser/fileapi/async_file_util.h"

class GURL;

namespace arc {

// Represents a file system root in Android Documents Provider.
//
// All methods must be called on the IO thread.
// If this object is deleted while there are in-flight operations, callbacks
// for those operations will be never called.
class ArcDocumentsProviderRoot {
 public:
  using GetFileInfoCallback = storage::AsyncFileUtil::GetFileInfoCallback;
  using ReadDirectoryCallback = storage::AsyncFileUtil::ReadDirectoryCallback;
  using ResolveToContentUrlCallback =
      base::Callback<void(const GURL& content_url)>;

  ArcDocumentsProviderRoot(const std::string& authority,
                           const std::string& root_document_id);
  ~ArcDocumentsProviderRoot();

  // Queries information of a file just like AsyncFileUtil.GetFileInfo().
  void GetFileInfo(const base::FilePath& path,
                   const GetFileInfoCallback& callback);

  // Queries a list of files under a directory just like
  // AsyncFileUtil.ReadDirectory().
  void ReadDirectory(const base::FilePath& path,
                     const ReadDirectoryCallback& callback);

  // Resolves a file path into a content:// URL pointing to the file
  // on DocumentsProvider. Returns URL that can be passed to
  // ArcContentFileSystemFileSystemReader to read the content.
  // On errors, an invalid GURL is returned.
  void ResolveToContentUrl(const base::FilePath& path,
                           const ResolveToContentUrlCallback& callback);

 private:
  // Thin representation of a document in documents provider.
  struct ThinDocument {
    std::string document_id;
    bool is_directory;
  };

  // Mapping from a file name to a ThinDocument.
  using NameToThinDocumentMap =
      std::map<base::FilePath::StringType, ThinDocument>;

  using ResolveToDocumentIdCallback =
      base::Callback<void(const std::string& document_id)>;
  using ReadDirectoryInternalCallback =
      base::Callback<void(base::File::Error error,
                          NameToThinDocumentMap mapping)>;

  void GetFileInfoWithDocumentId(const GetFileInfoCallback& callback,
                                 const std::string& document_id);
  void GetFileInfoWithDocument(const GetFileInfoCallback& callback,
                               mojom::DocumentPtr document);

  void ReadDirectoryWithDocumentId(const ReadDirectoryCallback& callback,
                                   const std::string& document_id);
  void ReadDirectoryWithNameToThinDocumentMap(
      const ReadDirectoryCallback& callback,
      base::File::Error error,
      NameToThinDocumentMap mapping);

  void ResolveToContentUrlWithDocumentId(
      const ResolveToContentUrlCallback& callback,
      const std::string& document_id);

  // Resolves |path| to a document ID. Failures are indicated by an empty
  // document ID.
  void ResolveToDocumentId(const base::FilePath& path,
                           const ResolveToDocumentIdCallback& callback);
  void ResolveToDocumentIdRecursively(
      const std::string& document_id,
      const std::vector<base::FilePath::StringType>& components,
      const ResolveToDocumentIdCallback& callback);
  void ResolveToDocumentIdRecursivelyWithNameToThinDocumentMap(
      const std::vector<base::FilePath::StringType>& components,
      const ResolveToDocumentIdCallback& callback,
      base::File::Error error,
      NameToThinDocumentMap mapping);

  // Enumerates child documents of a directory specified by |document_id|.
  // The result is returned as a NameToThinDocumentMap.
  void ReadDirectoryInternal(const std::string& document_id,
                             const ReadDirectoryInternalCallback& callback);
  void ReadDirectoryInternalWithChildDocuments(
      const ReadDirectoryInternalCallback& callback,
      base::Optional<std::vector<mojom::DocumentPtr>> maybe_children);

  const std::string authority_;
  const std::string root_document_id_;
  base::WeakPtrFactory<ArcDocumentsProviderRoot> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcDocumentsProviderRoot);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_ROOT_H_
