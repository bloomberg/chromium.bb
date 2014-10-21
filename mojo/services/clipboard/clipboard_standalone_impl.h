// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_CLIPBOARD_CLIPBOARD_STANDALONE_IMPL_H_
#define MOJO_SERVICES_CLIPBOARD_CLIPBOARD_STANDALONE_IMPL_H_

#include <base/memory/scoped_ptr.h>
#include <string>

#include "mojo/services/public/interfaces/clipboard/clipboard.mojom.h"

namespace mojo {

// Stub clipboard implementation.
//
// Eventually, we'll actually want to interact with the system clipboard, but
// that's hard today because the system clipboard is asynchronous (on X11), the
// ui::Clipboard interface is synchronous (which is what we'd use), mojo is
// asynchronous across processes, and the WebClipboard interface is synchronous
// (which is at least tractable).
class ClipboardStandaloneImpl : public InterfaceImpl<mojo::Clipboard> {
 public:
  // mojo::Clipboard exposes three possible clipboards.
  static const int kNumClipboards = 3;

  ClipboardStandaloneImpl();
  ~ClipboardStandaloneImpl() override;

  // InterfaceImpl<mojo::Clipboard> implementation.
  void GetSequenceNumber(
      Clipboard::Type clipboard_type,
      const mojo::Callback<void(uint64_t)>& callback) override;
  void GetAvailableMimeTypes(
      Clipboard::Type clipboard_types,
      const mojo::Callback<void(mojo::Array<mojo::String>)>& callback) override;
  void ReadMimeType(
      Clipboard::Type clipboard_type,
      const mojo::String& mime_type,
      const mojo::Callback<void(mojo::Array<uint8_t>)>& callback) override;
  void WriteClipboardData(
      Clipboard::Type clipboard_type,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> data) override;

 private:
  uint64_t sequence_number_[kNumClipboards];

  // Internal struct which stores the current state of the clipboard.
  class ClipboardData;

  // The current clipboard state. This is what is read from.
  scoped_ptr<ClipboardData> clipboard_state_[kNumClipboards];

  DISALLOW_COPY_AND_ASSIGN(ClipboardStandaloneImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_CLIPBOARD_CLIPBOARD_STANDALONE_IMPL_H_
