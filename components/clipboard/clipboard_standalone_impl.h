// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLIPBOARD_CLIPBOARD_STANDALONE_IMPL_H_
#define COMPONENTS_CLIPBOARD_CLIPBOARD_STANDALONE_IMPL_H_

#include <base/memory/scoped_ptr.h>
#include <string>

#include "components/clipboard/public/interfaces/clipboard.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace clipboard {

// Stub clipboard implementation.
//
// Eventually, we'll actually want to interact with the system clipboard, but
// that's hard today because the system clipboard is asynchronous (on X11), the
// ui::Clipboard interface is synchronous (which is what we'd use), mojo is
// asynchronous across processes, and the WebClipboard interface is synchronous
// (which is at least tractable).
class ClipboardStandaloneImpl : public mojo::Clipboard {
 public:
  // mojo::Clipboard exposes three possible clipboards.
  static const int kNumClipboards = 3;

  explicit ClipboardStandaloneImpl(
      mojo::InterfaceRequest<mojo::Clipboard> request);
  ~ClipboardStandaloneImpl() override;

  // mojo::Clipboard implementation.
  void GetSequenceNumber(
      mojo::Clipboard::Type clipboard_type,
      const mojo::Callback<void(uint64_t)>& callback) override;
  void GetAvailableMimeTypes(
      mojo::Clipboard::Type clipboard_types,
      const mojo::Callback<void(mojo::Array<mojo::String>)>& callback) override;
  void ReadMimeType(
      mojo::Clipboard::Type clipboard_type,
      const mojo::String& mime_type,
      const mojo::Callback<void(mojo::Array<uint8_t>)>& callback) override;
  void WriteClipboardData(
      mojo::Clipboard::Type clipboard_type,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> data) override;

 private:
  uint64_t sequence_number_[kNumClipboards];

  // Internal struct which stores the current state of the clipboard.
  class ClipboardData;

  // The current clipboard state. This is what is read from.
  scoped_ptr<ClipboardData> clipboard_state_[kNumClipboards];
  mojo::StrongBinding<mojo::Clipboard> binding_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardStandaloneImpl);
};

}  // namespace clipboard

#endif  // COMPONENTS_CLIPBOARD_CLIPBOARD_STANDALONE_IMPL_H_
