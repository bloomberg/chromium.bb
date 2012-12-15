/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "pepper_interface_mock.h"

PepperInterfaceMock::PepperInterfaceMock()
    : filesystem_interface_(new FileSystemInterfaceMock),
      fileref_interface_(new FileRefInterfaceMock),
      fileio_interface_(new FileIoInterfaceMock),
      directory_reader_interface_(new DirectoryReaderInterfaceMock),
      var_interface_(new VarInterfaceMock) {
}

PepperInterfaceMock::~PepperInterfaceMock() {
  delete filesystem_interface_;
  delete fileref_interface_;
  delete fileio_interface_;
  delete directory_reader_interface_;
  delete var_interface_;
}

FileSystemInterface* PepperInterfaceMock::GetFileSystemInterface() {
  return filesystem_interface_;
}

FileRefInterface* PepperInterfaceMock::GetFileRefInterface() {
  return fileref_interface_;
}

FileIoInterface* PepperInterfaceMock::GetFileIoInterface() {
  return fileio_interface_;
}

DirectoryReaderInterface* PepperInterfaceMock::GetDirectoryReaderInterface() {
  return directory_reader_interface_;
}

VarInterface* PepperInterfaceMock::GetVarInterface() {
  return var_interface_;
}
