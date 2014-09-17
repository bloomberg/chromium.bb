// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/clipboard/clipboard_standalone_impl.h"

namespace mojo {

typedef std::vector<uint8_t> ByteVector;

// ClipboardData contains data copied to the Clipboard for a variety of formats.
// It mostly just provides APIs to cleanly access and manipulate this data.
class ClipboardStandaloneImpl::ClipboardData {
 public:
  ClipboardData() {}
  ~ClipboardData() {}

  std::vector<std::string> GetMimeTypes() const {
    std::vector<std::string> types;
    for (std::map<std::string, ByteVector>::const_iterator it =
             data_types_.begin();
         it != data_types_.end();
         ++it) {
      types.push_back(it->first);
    }

    return types;
  }

  void SetData(std::map<std::string, ByteVector>* data) {
    std::swap(data_types_, *data);
  }

  bool GetData(const std::string& mime_type, ByteVector* data) const {
    std::map<std::string, ByteVector>::const_iterator it =
        data_types_.find(mime_type);
    if (it != data_types_.end()) {
      *data = it->second;
      return true;
    }

    return false;
  }

 private:
  std::map<std::string, ByteVector> data_types_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardData);
};

ClipboardStandaloneImpl::ClipboardStandaloneImpl() {
  for (int i = 0; i < kNumClipboards; ++i) {
    sequence_number_[i] = 0;
    clipboard_state_[i].reset(new ClipboardData);
  }
}

ClipboardStandaloneImpl::~ClipboardStandaloneImpl() {
}

void ClipboardStandaloneImpl::GetSequenceNumber(
    Clipboard::Type clipboard_type,
    const mojo::Callback<void(uint64_t)>& callback) {
  callback.Run(sequence_number_[clipboard_type]);
}

void ClipboardStandaloneImpl::GetAvailableMimeTypes(
    Clipboard::Type clipboard_type,
    const mojo::Callback<void(mojo::Array<mojo::String>)>& callback) {
  mojo::Array<mojo::String> types = mojo::Array<mojo::String>::From(
      clipboard_state_[clipboard_type]->GetMimeTypes());
  callback.Run(types.Pass());
}

void ClipboardStandaloneImpl::ReadMimeType(
    Clipboard::Type clipboard_type,
    const mojo::String& mime_type,
    const mojo::Callback<void(mojo::Array<uint8_t>)>& callback) {
  ByteVector mime_data;
  if (clipboard_state_[clipboard_type]->GetData(
          mime_type.To<std::string>(), &mime_data)) {
    callback.Run(mojo::Array<uint8_t>::From(mime_data).Pass());
    return;
  }

  callback.Run(mojo::Array<uint8_t>().Pass());
}

void ClipboardStandaloneImpl::WriteClipboardData(
    Clipboard::Type clipboard_type,
    mojo::Array<MimeTypePairPtr> data) {
  std::map<std::string, ByteVector> mime_data;
  for (size_t i = 0; i < data.size(); ++i)
    mime_data[data[i]->mime_type] = data[i]->data;

  sequence_number_[clipboard_type]++;
  clipboard_state_[clipboard_type]->SetData(&mime_data);
}

}  // namespace mojo
