// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_BUFFER_H_
#define MEDIA_BASE_DECODER_BUFFER_H_

#include "base/memory/aligned_memory.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

namespace media {

class DecryptConfig;

// A specialized buffer for interfacing with audio / video decoders.
//
// Specifically ensures that data is aligned and padded as necessary by the
// underlying decoding framework.  On desktop platforms this means memory is
// allocated using FFmpeg with particular alignment and padding requirements.
//
// Also includes decoder specific functionality for decryption.
class MEDIA_EXPORT DecoderBuffer
    : public base::RefCountedThreadSafe<DecoderBuffer> {
 public:
  enum {
    kPaddingSize = 16,
#if defined(ARCH_CPU_ARM_FAMILY)
    kAlignmentSize = 16
#else
    kAlignmentSize = 32
#endif
  };

  // Allocates buffer of size |buffer_size| >= 0.  Buffer will be padded and
  // aligned as necessary.
  explicit DecoderBuffer(int buffer_size);

  // Create a DecoderBuffer whose |data_| is copied from |data|.  Buffer will be
  // padded and aligned as necessary.  |data| must not be NULL and |size| >= 0.
  static scoped_refptr<DecoderBuffer> CopyFrom(const uint8* data, int size);

  // Create a DecoderBuffer indicating we've reached end of stream.  GetData()
  // and GetWritableData() will return NULL and GetDataSize() will return 0.
  static scoped_refptr<DecoderBuffer> CreateEOSBuffer();

  base::TimeDelta GetTimestamp() const;
  void SetTimestamp(const base::TimeDelta& timestamp);

  base::TimeDelta GetDuration() const;
  void SetDuration(const base::TimeDelta& duration);

  const uint8* GetData() const;
  uint8* GetWritableData() const;

  int GetDataSize() const;

  const DecryptConfig* GetDecryptConfig() const;
  void SetDecryptConfig(scoped_ptr<DecryptConfig> decrypt_config);

  // If there's no data in this buffer, it represents end of stream.
  //
  // TODO(scherkus): Change to be an actual settable flag as opposed to being
  // based on the value of |data_|, see http://crbug.com/169133
  bool IsEndOfStream() const;

 protected:
  friend class base::RefCountedThreadSafe<DecoderBuffer>;

  // Allocates a buffer of size |size| >= 0 and copies |data| into it.  Buffer
  // will be padded and aligned as necessary.  If |data| is NULL then |data_| is
  // set to NULL and |buffer_size_| to 0.
  DecoderBuffer(const uint8* data, int size);
  virtual ~DecoderBuffer();

 private:
  base::TimeDelta timestamp_;
  base::TimeDelta duration_;

  int buffer_size_;
  scoped_ptr<uint8, base::ScopedPtrAlignedFree> data_;
  scoped_ptr<DecryptConfig> decrypt_config_;

  // Constructor helper method for memory allocations.
  void Initialize();

  DISALLOW_COPY_AND_ASSIGN(DecoderBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_H_
