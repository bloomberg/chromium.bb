// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_STORE_H_
#define COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_STORE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/sync/model/attachments/attachment.h"
#include "components/sync/model/attachments/attachment_id.h"
#include "components/sync/model/attachments/attachment_metadata.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace syncer {

class AttachmentStoreBackend;
class AttachmentStoreForSync;
class AttachmentStoreFrontend;

// AttachmentStore is a place to locally store and access Attachments.
//
// AttachmentStore class is an interface exposed to data type and
// AttachmentService code.
// It also contains factory methods for default attachment store
// implementations.
// Destroying this object does not necessarily cancel outstanding async
// operations. If you need cancel like semantics, use WeakPtr in the callbacks.
class AttachmentStore {
 public:
  // TODO(maniscalco): Consider udpating Read and Write methods to support
  // resumable transfers (bug 353292).

  // The result status of an attachment store operation.
  // Do not re-order or delete these entries; they are used in a UMA histogram.
  enum Result {
    SUCCESS = 0,            // No error, all completed successfully.
    UNSPECIFIED_ERROR = 1,  // An unspecified error occurred for >= 1 item.
    STORE_INITIALIZATION_FAILED = 2,  // AttachmentStore initialization failed.
    // When adding a value here, you must increment RESULT_SIZE below.
  };
  static const int RESULT_SIZE =
      10;  // Size of the Result enum; used for histograms.

  // Each attachment can have references from sync or model type. Tracking these
  // references is needed for lifetime management of attachment, it can only be
  // deleted from the store when it doesn't have references.
  enum Component {
    MODEL_TYPE,
    SYNC,
  };

  using InitCallback = base::Callback<void(const Result&)>;
  using ReadCallback = base::Callback<void(const Result&,
                                           std::unique_ptr<AttachmentMap>,
                                           std::unique_ptr<AttachmentIdList>)>;
  using WriteCallback = base::Callback<void(const Result&)>;
  using DropCallback = base::Callback<void(const Result&)>;
  using ReadMetadataCallback =
      base::Callback<void(const Result&,
                          std::unique_ptr<AttachmentMetadataList>)>;

  ~AttachmentStore();

  // Asynchronously reads the attachments identified by |ids|.
  //
  // |callback| will be invoked when finished. AttachmentStore will attempt to
  // read all attachments specified in ids. If any of the attachments do not
  // exist or could not be read, |callback|'s Result will be UNSPECIFIED_ERROR.
  // Callback's AttachmentMap will contain all attachments that were
  // successfully read, AttachmentIdList will contain attachment ids of
  // attachments that are unavailable in attachment store, these need to be
  // downloaded from server.
  //
  // Reads on individual attachments are treated atomically; |callback| will not
  // read only part of an attachment.
  void Read(const AttachmentIdList& ids, const ReadCallback& callback);

  // Asynchronously writes |attachments| to the store.
  //
  // Will not overwrite stored attachments. Attempting to overwrite an
  // attachment that already exists is not an error.
  //
  // |callback| will be invoked when finished. If any of the attachments could
  // not be written |callback|'s Result will be UNSPECIFIED_ERROR. When this
  // happens, some or none of the attachments may have been written
  // successfully.
  void Write(const AttachmentList& attachments, const WriteCallback& callback);

  // Asynchronously drops |attchments| from this store.
  //
  // This does not remove attachments from the server.
  //
  // |callback| will be invoked when finished. Attempting to drop an attachment
  // that does not exist is not an error. If any of the existing attachment
  // could not be dropped, |callback|'s Result will be UNSPECIFIED_ERROR. When
  // this happens, some or none of the attachments may have been dropped
  // successfully.
  void Drop(const AttachmentIdList& ids, const DropCallback& callback);

  // Asynchronously reads metadata for the attachments identified by |ids|.
  //
  // |callback| will be invoked when finished. AttachmentStore will attempt to
  // read metadata for all attachments specified in ids. If any of the
  // metadata entries do not exist or could not be read, |callback|'s Result
  // will be UNSPECIFIED_ERROR.
  void ReadMetadataById(const AttachmentIdList& ids,
                        const ReadMetadataCallback& callback);

  // Asynchronously reads metadata for all attachments with |component_|
  // reference in the store.
  //
  // |callback| will be invoked when finished. If any of the metadata entries
  // could not be read, |callback|'s Result will be UNSPECIFIED_ERROR.
  void ReadMetadata(const ReadMetadataCallback& callback);

  // Given current AttachmentStore (this) creates separate AttachmentStore that
  // will be used by sync components (AttachmentService). Resulting
  // AttachmentStore is backed by the same frontend/backend.
  std::unique_ptr<AttachmentStoreForSync> CreateAttachmentStoreForSync() const;

  // Creates an AttachmentStore backed by in-memory implementation of attachment
  // store. For now frontend lives on the same thread as backend.
  static std::unique_ptr<AttachmentStore> CreateInMemoryStore();

  // Creates an AttachmentStore backed by on-disk implementation of attachment
  // store. Opens corresponding leveldb database located at |path|. All backend
  // operations are scheduled to |backend_task_runner|. Opening attachment store
  // is asynchronous, once it finishes |callback| will be called on the thread
  // that called CreateOnDiskStore. Calling Read/Write/Drop before
  // initialization completed is allowed.  Later if initialization fails these
  // operations will fail with STORE_INITIALIZATION_FAILED error.
  static std::unique_ptr<AttachmentStore> CreateOnDiskStore(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
      const InitCallback& callback);

  // Creates set of AttachmentStore/AttachmentStoreFrontend instances for tests
  // that provide their own implementation of AttachmentstoreBackend for
  // mocking.
  static std::unique_ptr<AttachmentStore> CreateMockStoreForTest(
      std::unique_ptr<AttachmentStoreBackend> backend);

 protected:
  AttachmentStore(const scoped_refptr<AttachmentStoreFrontend>& frontend,
                  Component component);

  const scoped_refptr<AttachmentStoreFrontend>& frontend() { return frontend_; }
  Component component() const { return component_; }

 private:
  scoped_refptr<AttachmentStoreFrontend> frontend_;
  // Modification operations with attachment store will be performed on behalf
  // of |component_|.
  const Component component_;

  DISALLOW_COPY_AND_ASSIGN(AttachmentStore);
};

// AttachmentStoreForSync extends AttachmentStore and provides additional
// functions necessary for AttachmentService. These are needed when
// AttachmentService writes attachment on behalf of model type after download
// and takes reference on attachment for the duration of upload.
// Model type implementation shouldn't use this interface.
class AttachmentStoreForSync : public AttachmentStore {
 public:
  ~AttachmentStoreForSync();

  // Asynchronously adds reference from sync to attachments.
  void SetSyncReference(const AttachmentIdList& ids);

  // Asynchronously adds reference from model type to attachments.
  // Needed in GetOrDownloadAttachments when attachment is in local store but
  // doesn't have model type reference.
  void SetModelTypeReference(const AttachmentIdList& ids);

  // Asynchronously drops sync reference from attachments.
  void DropSyncReference(const AttachmentIdList& ids);

  // Asynchronously reads metadata for all attachments with |sync_component_|
  // reference in the store.
  //
  // |callback| will be invoked when finished. If any of the metadata entries
  // could not be read, |callback|'s Result will be UNSPECIFIED_ERROR.
  void ReadMetadataForSync(const ReadMetadataCallback& callback);

 private:
  friend class AttachmentStore;
  AttachmentStoreForSync(const scoped_refptr<AttachmentStoreFrontend>& frontend,
                         Component consumer_component,
                         Component sync_component);

  // |sync_component_| is passed to frontend when sync related operations are
  // perfromed.
  const Component sync_component_;

  DISALLOW_COPY_AND_ASSIGN(AttachmentStoreForSync);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_STORE_H_
