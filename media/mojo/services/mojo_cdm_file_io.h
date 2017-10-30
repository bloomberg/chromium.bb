// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_FILE_IO_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_FILE_IO_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/cdm_file_io.h"
#include "media/mojo/interfaces/cdm_storage.mojom.h"
#include "media/mojo/services/media_mojo_export.h"

namespace media {

// TODO(crbug.com/774160): This class should do the Read/Write operations on a
// separate thread so as to not impact decoding happening on the same thread.
// The new thread may need to block shutdown so that the file is not corrupted.

// Implements a CdmFileIO that communicates with mojom::CdmStorage.
class MEDIA_MOJO_EXPORT MojoCdmFileIO : public CdmFileIO {
 public:
  MojoCdmFileIO(cdm::FileIOClient* client, mojom::CdmStorage* cdm_storage);
  ~MojoCdmFileIO() override;

  // CdmFileIO implementation.
  void Open(const char* file_name, uint32_t file_name_size) final;
  void Read() final;
  void Write(const uint8_t* data, uint32_t data_size) final;
  void Close() final;

 private:
  // Allowed state transitions:
  //   kUnopened -> kOpening -> kOpened
  //   kUnopened -> kOpening -> kUnopened (if file in use)
  //   kUnopened -> kOpening -> kError (if file not available)
  //   kOpened -> kReading -> kOpened
  //   kOpened -> kWriting -> kOpened
  // Once state = kError, only Close() can be called.
  enum class State { kUnopened, kOpening, kOpened, kReading, kWriting, kError };

  // Error that needs to be reported back to the client.
  enum class ErrorType {
    kOpenError,
    kOpenInUse,
    kReadError,
    kReadInUse,
    kWriteError,
    kWriteInUse
  };

  // Called when the file is opened (or not).
  void OnFileOpened(mojom::CdmStorage::Status status,
                    base::File file,
                    mojom::CdmFileAssociatedPtrInfo cdm_file);

  // Reading the file is done asynchronously.
  void DoRead(int64_t num_bytes);

  // Called when a temporary file has been opened for writing.
  void OnFileOpenedForWriting(const std::vector<uint8_t>& data,
                              base::File temporary_file);

  // Called after the write has been committed and replaces the original file.
  void OnWriteCommitted(base::File reopened_file);

  // Called when an error occurs. Calls client_->OnXxxxComplete with kError
  // or kInUse asynchronously. In some cases we could actually call them
  // synchronously, but since these errors shouldn't happen in normal cases,
  // we are not optimizing such cases.
  void OnError(ErrorType error);

  // Callback to notify client of error asynchronously.
  void NotifyClientOfError(ErrorType error);

  // Results of CdmFileIO operations are sent asynchronously via |client_|.
  cdm::FileIOClient* client_;

  mojom::CdmStorage* cdm_storage_;

  // Keep track of the file being used. As this class can only be used for
  // accessing a single file, once |file_name_| is set it shouldn't be changed.
  // |file_name_| is only saved for logging purposes.
  std::string file_name_;

  // Current file open for reading.
  base::File file_for_reading_;

  // |cdm_file_| is used to write to the file and is released when the file is
  // closed so that CdmStorage can tell that the file is no longer being used.
  mojom::CdmFileAssociatedPtr cdm_file_;

  // Keep track of operations in progress.
  State state_ = State::kUnopened;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MojoCdmFileIO> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoCdmFileIO);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_FILE_IO_H_
