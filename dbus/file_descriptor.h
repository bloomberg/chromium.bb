// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_FILE_DESCRIPTOR_H_
#define DBUS_FILE_DESCRIPTOR_H_
#pragma once

namespace dbus {

// FileDescriptor is a type used to encapsulate D-Bus file descriptors
// and to follow the RAII idiom appropiate for use with message operations
// where the descriptor might be easily leaked.  To guard against this the
// descriptor is closed when an instance is destroyed if it is owned.
// Ownership is asserted only when PutValue is used and TakeValue can be
// used to take ownership.
//
// For example, in the following
//  FileDescriptor fd;
//  if (!reader->PopString(&name) ||
//      !reader->PopFileDescriptor(&fd) ||
//      !reader->PopUint32(&flags)) {
// the descriptor in fd will be closed if the PopUint32 fails.  But
//   writer.AppendFileDescriptor(dbus::FileDescriptor(1));
// will not automatically close "1" because it is not owned.
class FileDescriptor {
 public:
  // Permits initialization without a value for passing to
  // dbus::MessageReader::PopFileDescriptor to fill in and from int values.
  FileDescriptor() : value_(-1), owner_(false) {}
  explicit FileDescriptor(int value) : value_(value), owner_(false) {}

  virtual ~FileDescriptor();

  // Retrieves value as an int without affecting ownership.
  int value() const { return value_; }

  // Sets the value and assign ownership.
  void PutValue(int value) {
    value_ = value;
    owner_ = true;
  }

  // Takes the value and ownership.
  int TakeValue() {
    owner_ = false;
    return value_;
  }

 private:
  int value_;
  bool owner_;

  DISALLOW_COPY_AND_ASSIGN(FileDescriptor);
};

}  // namespace dbus

#endif  // DBUS_FILE_DESCRIPTOR_H_
