// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_NFC_NFC_TAG_H_
#define DEVICE_NFC_NFC_TAG_H_

#include "device/nfc/nfc_tag_technology.h"

namespace device {

// NfcTag represents a remote NFC tag. An NFC tag is a passive NFC device,
// powered by the NFC field of the local adapter while it is in range. Tags
// can come in many forms, such as stickers, key fobs, or even embedded in a
// more sofisticated device.
//
// Tags can have a wide range of capabilities. Simple tags just offer
// read/write semantics, and contain some one time programmable areas to make
// read-only. More complex tags offer math operations and per-sector access
// control and authentication. The most sophisticated tags contain operating
// environments allowing complex interactions with the code executing on the
// tag.
//
// The NfcTag class facilitates possible interactions with a tag. The most
// common usage of a tag is to exchange NDEF messages, but different kinds of
// I/O can be performed using the NfcTagTechnology classes.
class NfcTag {
 public:
  // NFC tag types.
  enum TagType {
    kTagType1,
    kTagType2,
    kTagType3,
    kTagType4
  };

  // NFC protocols that a tag can support. A tag will usually support only one
  // of these.
  enum Protocol {
    kProtocolFelica,
    kProtocolIsoDep,
    kProtocolJewel,
    kProtocolMifare,
    kProtocolNfcDep
  };

  // Interface for observing changes from NFC tags.
  class Observer {
   public:
    virtual ~Observer() {}

    // This method will be called when an NDEF message |message|, stored on the
    // NFC tag |tag| has been read. Although NDEF is the most common record
    // storage format for NFC tags, not all tags support it. This method won't
    // be called if there are no records on an NDEF compliant tag or if the tag
    // doesn't support NDEF.
    virtual void RecordsReceived(NfcTag* tag, const NfcNdefMessage& message) {}
  };

  virtual ~NfcTag();

  // Adds and removes observers for events on this NFC tag. If monitoring
  // multiple tags, check the |tag| parameter of observer methods to determine
  // which tag is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the unique identifier assigned to this tag.
  virtual std::string GetIdentifier() const = 0;

  // Returns the current tag's NFC forum specified "type".
  virtual TagType GetType() const = 0;

  // Returns true, if this tag is read-only and cannot be written to.
  virtual bool IsReadOnly() const = 0;

  // Returns the current tag's supported NFC protocol.
  virtual Protocol GetSupportedProtocol() const = 0;

  // Returns a bitmask of the tag I/O technologies supported by this tag.
  virtual NfcTagTechnology::TechnologyTypeMask
      GetSupportedTechnologies() const = 0;

 protected:
  NfcTag();

 private:
  DISALLOW_COPY_AND_ASSIGN(NfcTag);
};

}  // namespace device

#endif  // DEVICE_NFC_NFC_TAG_H_
