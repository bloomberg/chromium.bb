// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/message.h"

#include <assert.h>
#include <stdlib.h>

#include <algorithm>

namespace mojo {

Message::Message()
    : data_num_bytes_(0),
      data_(NULL) {
}

Message::~Message() {
  free(data_);

  for (std::vector<Handle>::iterator it = handles_.begin();
       it != handles_.end(); ++it) {
    if (it->is_valid())
      CloseRaw(*it);
  }
}

void Message::AllocUninitializedData(uint32_t num_bytes) {
  assert(!data_);
  data_num_bytes_ = num_bytes;
  data_ = static_cast<internal::MessageData*>(malloc(num_bytes));
}

void Message::AdoptData(uint32_t num_bytes, internal::MessageData* data) {
  assert(!data_);
  data_num_bytes_ = num_bytes;
  data_ = data;
}

void Message::Swap(Message* other) {
  std::swap(data_num_bytes_, other->data_num_bytes_);
  std::swap(data_, other->data_);
  std::swap(handles_, other->handles_);
}

}  // namespace mojo
