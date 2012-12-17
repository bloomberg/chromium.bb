/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "pepper_interface_mock.h"

PepperInterfaceMock::PepperInterfaceMock(PP_Instance instance)
    : instance_(instance),
      console_interface_(new ConsoleInterfaceMock),
      filesystem_interface_(new FileSystemInterfaceMock),
      fileref_interface_(new FileRefInterfaceMock),
      fileio_interface_(new FileIoInterfaceMock),
      directory_reader_interface_(new DirectoryReaderInterfaceMock),
      messaging_interface_(new MessagingInterfaceMock),
      var_interface_(new VarInterfaceMock) {
}

PepperInterfaceMock::~PepperInterfaceMock() {
  delete console_interface_;
  delete filesystem_interface_;
  delete fileref_interface_;
  delete fileio_interface_;
  delete directory_reader_interface_;
  delete messaging_interface_;
  delete var_interface_;
}

PP_Instance PepperInterfaceMock::GetInstance() {
  return instance_;
}

ConsoleInterfaceMock* PepperInterfaceMock::GetConsoleInterface() {
  return console_interface_;
}

FileSystemInterfaceMock* PepperInterfaceMock::GetFileSystemInterface() {
  return filesystem_interface_;
}

FileRefInterfaceMock* PepperInterfaceMock::GetFileRefInterface() {
  return fileref_interface_;
}

FileIoInterfaceMock* PepperInterfaceMock::GetFileIoInterface() {
  return fileio_interface_;
}

DirectoryReaderInterfaceMock*
PepperInterfaceMock::GetDirectoryReaderInterface() {
  return directory_reader_interface_;
}

MessagingInterfaceMock* PepperInterfaceMock::GetMessagingInterface() {
  return messaging_interface_;
}

VarInterfaceMock* PepperInterfaceMock::GetVarInterface() {
  return var_interface_;
}

ConsoleInterfaceMock::ConsoleInterfaceMock() {
}

ConsoleInterfaceMock::~ConsoleInterfaceMock() {
}

FileSystemInterfaceMock::FileSystemInterfaceMock() {
}

FileSystemInterfaceMock::~FileSystemInterfaceMock() {
}

FileRefInterfaceMock::FileRefInterfaceMock() {
}

FileRefInterfaceMock::~FileRefInterfaceMock() {
}

FileIoInterfaceMock::FileIoInterfaceMock() {
}

FileIoInterfaceMock::~FileIoInterfaceMock() {
}

DirectoryReaderInterfaceMock::DirectoryReaderInterfaceMock() {
}

DirectoryReaderInterfaceMock::~DirectoryReaderInterfaceMock() {
}

MessagingInterfaceMock::MessagingInterfaceMock() {
}

MessagingInterfaceMock::~MessagingInterfaceMock() {
}

VarInterfaceMock::VarInterfaceMock() {
}

VarInterfaceMock::~VarInterfaceMock() {
}
