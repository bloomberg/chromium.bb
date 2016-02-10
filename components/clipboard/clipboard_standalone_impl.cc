// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/clipboard/clipboard_standalone_impl.h"

#include <string.h>
#include <utility>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/bindings/string.h"

using mojo::Array;
using mojo::Map;
using mojo::String;

namespace clipboard {

// ClipboardData contains data copied to the Clipboard for a variety of formats.
// It mostly just provides APIs to cleanly access and manipulate this data.
class ClipboardStandaloneImpl::ClipboardData {
 public:
  ClipboardData() {}
  ~ClipboardData() {}

  Array<String> GetMimeTypes() const {
    Array<String> types(data_types_.size());
    int i = 0;
    for (auto it = data_types_.begin(); it != data_types_.end(); ++it, ++i)
      types[i] = it->first;

    return types;
  }

  void SetData(Map<String, Array<uint8_t>> data) {
    data_types_ = std::move(data);
  }

  void GetData(const String& mime_type, Array<uint8_t>* data) const {
    auto it = data_types_.find(mime_type);
    if (it != data_types_.end())
      *data = it->second.Clone();
  }

 private:
  Map<String, Array<uint8_t>> data_types_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardData);
};

ClipboardStandaloneImpl::ClipboardStandaloneImpl(
    mojo::InterfaceRequest<mojo::Clipboard> request)
    : binding_(this, std::move(request)) {
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
  callback.Run(sequence_number_[static_cast<int>(clipboard_type)]);
}

void ClipboardStandaloneImpl::GetAvailableMimeTypes(
    Clipboard::Type clipboard_type,
    const mojo::Callback<void(Array<String>)>& callback) {
  callback.Run(
      clipboard_state_[static_cast<int>(clipboard_type)]->GetMimeTypes());
}

void ClipboardStandaloneImpl::ReadMimeType(
    Clipboard::Type clipboard_type,
    const String& mime_type,
    const mojo::Callback<void(Array<uint8_t>)>& callback) {
  Array<uint8_t> mime_data;
  clipboard_state_[static_cast<int>(clipboard_type)]->GetData(mime_type,
                                                              &mime_data);
  callback.Run(std::move(mime_data));
}

void ClipboardStandaloneImpl::WriteClipboardData(
    Clipboard::Type clipboard_type,
    Map<String, Array<uint8_t>> data) {
  sequence_number_[static_cast<int>(clipboard_type)]++;
  clipboard_state_[static_cast<int>(clipboard_type)]->SetData(std::move(data));
}

}  // namespace clipboard
