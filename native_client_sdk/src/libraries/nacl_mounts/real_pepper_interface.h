/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_MOUNTS_REAL_PEPPER_INTERFACE_H_
#define LIBRARIES_NACL_MOUNTS_REAL_PEPPER_INTERFACE_H_

#include <ppapi/c/ppb.h>
#include <ppapi/c/ppb_core.h>
#include <ppapi/c/ppb_console.h>
#include <ppapi/c/ppb_message_loop.h>
#include <ppapi/c/ppb_messaging.h>
#include <ppapi/c/ppb_var.h>
#include "pepper_interface.h"

class RealConsoleInterface;
class RealDirectoryReaderInterface;
class RealFileIoInterface;
class RealFileRefInterface;
class RealFileSystemInterface;
class RealMessagingInterface;
class RealVarInterface;

class RealPepperInterface : public PepperInterface {
 public:
  RealPepperInterface(PP_Instance instance,
                      PPB_GetInterface get_browser_interface);

  virtual PP_Instance GetInstance();
  virtual void AddRefResource(PP_Resource);
  virtual void ReleaseResource(PP_Resource);
  virtual ConsoleInterface* GetConsoleInterface();
  virtual FileSystemInterface* GetFileSystemInterface();
  virtual FileRefInterface* GetFileRefInterface();
  virtual FileIoInterface* GetFileIoInterface();
  virtual DirectoryReaderInterface* GetDirectoryReaderInterface();
  virtual MessagingInterface* GetMessagingInterface();
  virtual VarInterface* GetVarInterface();

  int32_t InitializeMessageLoop();

 private:
  PP_Instance instance_;
  const PPB_Core* core_interface_;
  const PPB_MessageLoop* message_loop_interface_;
  RealConsoleInterface* console_interface_;
  RealDirectoryReaderInterface* directory_reader_interface_;
  RealFileIoInterface* fileio_interface_;
  RealFileRefInterface* fileref_interface_;
  RealFileSystemInterface* filesystem_interface_;
  RealMessagingInterface* messaging_interface_;
  RealVarInterface* var_interface_;
};

#endif  // LIBRARIES_NACL_MOUNTS_REAL_PEPPER_INTERFACE_H_
