// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_CDM_FILE_IO_IMPL_H_
#define MEDIA_CDM_PPAPI_CDM_FILE_IO_IMPL_H_

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "media/cdm/ppapi/api/content_decryption_module.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/isolated_file_system_private.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace media {

// Due to PPAPI limitations, all functions must be called on the main thread.
class CdmFileIOImpl : public cdm::FileIO {
 public:
  CdmFileIOImpl(cdm::FileIOClient* client, PP_Instance pp_instance);
  virtual ~CdmFileIOImpl();

  // cdm::FileIO implementation.
  virtual void Open(const char* file_name, uint32_t file_name_size) OVERRIDE;
  virtual void Read() OVERRIDE;
  virtual void Write(const uint8_t* data, uint32_t data_size) OVERRIDE;
  virtual void Close() OVERRIDE;

 private:
  enum State {
    FILE_UNOPENED,
    OPENING_FILE_SYSTEM,
    OPENING_FILE,
    FILE_OPENED,
    READING_FILE,
    WRITING_FILE,
    FILE_CLOSED
  };

  enum ErrorType {
    OPEN_WHILE_IN_USE,
    READ_WHILE_IN_USE,
    WRITE_WHILE_IN_USE,
    OPEN_ERROR,
    READ_ERROR,
    WRITE_ERROR
  };

  void OpenFileSystem();
  void OnFileSystemOpened(int32_t result, pp::FileSystem file_system);
  void OpenFile();
  void OnFileOpened(int32_t result);
  void ReadFile();
  void OnFileRead(int32_t bytes_read);
  void SetLength(uint32_t length);
  void OnLengthSet(int32_t result);
  void WriteFile();
  void OnFileWritten(int32_t bytes_written);

  void CloseFile();

  // Calls client_->OnXxxxComplete with kError asynchronously. In some cases we
  // could actually call them synchronously, but since these errors shouldn't
  // happen in normal cases, we are not optimizing such cases.
  void OnError(ErrorType error_type);
  // Callback to notify client of error asynchronously.
  void NotifyClientOfError(int32_t result, ErrorType error_type);

  State state_;

  // Non-owning pointer.
  cdm::FileIOClient* const client_;

  const pp::InstanceHandle pp_instance_handle_;

  std::string file_name_;
  pp::IsolatedFileSystemPrivate isolated_file_system_;
  pp::FileSystem file_system_;
  pp::FileIO file_io_;
  pp::FileRef file_ref_;

  pp::CompletionCallbackFactory<CdmFileIOImpl> callback_factory_;

  // A temporary buffer to hold (partial) data to write or the data that has
  // been read. The size of |io_buffer_| is always "bytes to write" or "bytes to
  // read". Use "char" instead of "unit8_t" because PPB_FileIO uses char* for
  // binary data read and write.
  std::vector<char> io_buffer_;

  // Offset into the file for reading/writing data. When writing data to the
  // file, this is also the offset to the |io_buffer_|.
  size_t io_offset_;

  // Buffer to hold all read data requested. This buffer is passed to |client_|
  // when read completes.
  std::vector<char> cumulative_read_buffer_;

  DISALLOW_COPY_AND_ASSIGN(CdmFileIOImpl);
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_CDM_FILE_IO_IMPL_H_
