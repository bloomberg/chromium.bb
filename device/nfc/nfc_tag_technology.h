// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_NFC_NFC_TAG_TECHNOLOGY_H_
#define DEVICE_NFC_NFC_TAG_TECHNOLOGY_H_

#include "base/callback.h"
#include "device/nfc/nfc_ndef_record.h"

namespace device {

class NfcTag;

// NfcTagTechnology represents an NFC technology that allows a certain type of
// I/O operation on an NFC tag. NFC tags can support a wide array of protocols.
// The NfcTagTechnology hierarchy allows both raw and high-level I/O operations
// on NFC tags.
class NfcTagTechnology {
 public:
  // The various I/O technologies that an NFC tag can support.
  enum TechnologyType {
    kTechnologyTypeNfcA = 1 << 0,
    kTechnologyTypeNfcB = 1 << 1,
    kTechnologyTypeNfcF = 1 << 2,
    kTechnologyTypeNfcV = 1 << 3,
    kTechnologyTypeIsoDep = 1 << 4,
    kTechnologyTypeNdef = 1 << 5
  };
  typedef uint32 TechnologyTypeMask;

  virtual ~NfcTagTechnology();

  // Returns true, if the underlying tag supports the NFC tag technology that
  // this instance represents.
  virtual bool IsSupportedByTag() const = 0;

  // Returns a pointer to the associated NfcTag instance.
  NfcTag* tag() const { return tag_; }

 protected:
  // Constructs a technology instance, where |tag| is the NFC tag that this
  // instance will operate on. Clients aren't allowed to instantiate classes
  // directly. They should use the static "Create" methods defined in each
  // subclass to obtain the platform specific implementation.
  explicit NfcTagTechnology(NfcTag* tag);

 private:
  NfcTagTechnology();

  // The underlying NfcTag instance that data exchange operations through this
  // instance are performed on.
  NfcTag* tag_;

  DISALLOW_COPY_AND_ASSIGN(NfcTagTechnology);
};

// NfcNdefTagTechnology allows reading and writing NDEF messages to a tag. This
// is the most commonly used data exchange format in NFC.
class NfcNdefTagTechnology : public NfcTagTechnology {
 public:
  // The ErrorCallback is used by methods to asynchronously report errors.
  typedef base::Closure ErrorCallback;

  virtual ~NfcNdefTagTechnology();

  // NfcTagTechnology override.
  virtual bool IsSupportedByTag() const OVERRIDE;

  // Returns all NDEF records that were received from the tag in the form of an
  // NDEF message. If the returned NDEF message contains no records, this only
  // means that no records have yet been received from the tag. Users should
  // use this method in conjunction with the NfcTag::Observer::RecordsReceived
  // method to be notified when the records are ready.
  virtual NfcNdefMessage GetNdefMessage() const = 0;

  // Writes the given NDEF message to the underlying tag, overwriting any
  // existing NDEF message on it. On success, |callback| will be invoked. On
  // failure, |error_callback| will be invoked. This method can fail, if the
  // underlying tag does not support NDEF as a technology.
  virtual void WriteNdefMessage(const NfcNdefMessage& message,
                                const base::Closure& callback,
                                const ErrorCallback& error_callback) = 0;

  // Static factory method for constructing an instance. The ownership of the
  // returned instance belongs to the caller. Returns NULL, if NFC is not
  // supported on the current platform.
  static NfcNdefTagTechnology* Create(NfcTag* tag);

 private:
  // Constructs a technology instance, where |tag| is the NFC tag that this
  // instance will operate on.
  explicit NfcNdefTagTechnology(NfcTag* tag);

  DISALLOW_COPY_AND_ASSIGN(NfcNdefTagTechnology);
};

}  // namespace device

#endif  // DEVICE_NFC_NFC_TAG_TECHNOLOGY_H_
