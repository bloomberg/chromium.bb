// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/ppapi/external_clear_key/cdm_file_adapter.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"

namespace media {

namespace {

CdmFileAdapter::Status ConvertStatus(cdm::FileIOClient::Status status) {
  switch (status) {
    case cdm::FileIOClient::kSuccess:
      return CdmFileAdapter::Status::kSuccess;
    case cdm::FileIOClient::kInUse:
      return CdmFileAdapter::Status::kInUse;
    case cdm::FileIOClient::kError:
      return CdmFileAdapter::Status::kError;
  }

  NOTREACHED();
  return CdmFileAdapter::Status::kError;
}

}  // namespace

CdmFileAdapter::CdmFileAdapter(cdm::ContentDecryptionModule_9::Host* host) {
  file_io_ = host->CreateFileIO(this);
}

CdmFileAdapter::~CdmFileAdapter() {
  DCHECK(open_cb_.is_null());
  DCHECK(read_cb_.is_null());
  DCHECK(write_cb_.is_null());
  file_io_->Close();
}

void CdmFileAdapter::Open(const std::string& name, FileOpenedCB open_cb) {
  DVLOG(2) << __func__;
  open_cb_ = std::move(open_cb);
  file_io_->Open(name.data(), name.length());
}

void CdmFileAdapter::Read(ReadCB read_cb) {
  DVLOG(2) << __func__;
  read_cb_ = std::move(read_cb);
  file_io_->Read();
}

void CdmFileAdapter::Write(const std::vector<uint8_t>& data, WriteCB write_cb) {
  DVLOG(2) << __func__;
  write_cb_ = std::move(write_cb);
  file_io_->Write(data.data(), data.size());
}

void CdmFileAdapter::OnOpenComplete(cdm::FileIOClient::Status status) {
  std::move(open_cb_).Run(ConvertStatus(status));
}

void CdmFileAdapter::OnReadComplete(cdm::FileIOClient::Status status,
                                    const uint8_t* data,
                                    uint32_t data_size) {
  std::move(read_cb_).Run(status == kSuccess && data_size > 0,
                          std::vector<uint8_t>(data, data + data_size));
}

void CdmFileAdapter::OnWriteComplete(cdm::FileIOClient::Status status) {
  std::move(write_cb_).Run(status == kSuccess);
}

}  // namespace media
