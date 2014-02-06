// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/devfs/jspipe_node.h"

//#include <cstdio>
#include <cstring>

#include "nacl_io/devfs/dev_fs.h"
#include "nacl_io/error.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/pepper_interface.h"

namespace nacl_io {

#define MAX_MESSAGE_SIZE (64*1024)
Error JSPipeNode::Write(const HandleAttr& attr,
                        const void* buf,
                        size_t count,
                        int* out_bytes) {
  PepperInterface* ppapi = filesystem_->ppapi();
  MessagingInterface* iface = ppapi->GetMessagingInterface();
  VarInterface* var_iface = ppapi->GetVarInterface();
  VarArrayInterface* array_iface = ppapi->GetVarArrayInterface();
  VarArrayBufferInterface* buffer_iface = ppapi->GetVarArrayBufferInterface();
  if (!iface || !var_iface || !array_iface || !buffer_iface)
    return ENOSYS;

  if (name_.empty())
    return EIO;

  // Limit the size of the data we send with PostMessage to MAX_MESSAGE_SIZE
  if (count > MAX_MESSAGE_SIZE)
      count = MAX_MESSAGE_SIZE;

  // Copy data in a new ArrayBuffer
  PP_Var buffer = buffer_iface->Create(count);
  memcpy(buffer_iface->Map(buffer), buf, count);
  buffer_iface->Unmap(buffer);

  // Construct string var containing the name of the pipe
  PP_Var string = var_iface->VarFromUtf8(name_.c_str(), name_.size());

  // Construct two element array containing string and array buffer
  PP_Var array = array_iface->Create();
  array_iface->Set(array, 0, string);
  array_iface->Set(array, 1, buffer);

  // Release our handles to the items that are now in the array
  var_iface->Release(string);
  var_iface->Release(buffer);

  // Send array via PostMessage
  iface->PostMessage(ppapi->GetInstance(), array);

  // Release the array
  var_iface->Release(array);

  *out_bytes = count;
  return 0;
}

Error JSPipeNode::VIoctl(int request, va_list args) {
  switch (request) {
    case TIOCNACLPIPENAME: {
      // name can only be set once
      if (!name_.empty())
        return EIO;

      const char* new_name = va_arg(args, char*);
      // new name must not be empty
      if (!new_name || strlen(new_name) == 0)
        return EIO;

      name_ = new_name;
      return 0;
    }
    case TIOCNACLINPUT: {
      // This ioctl is used to deliver data from javascipt into the pipe.
      struct tioc_nacl_input_string* message =
          va_arg(args, struct tioc_nacl_input_string*);
      const char* buffer = message->buffer;
      int num_bytes = message->length;
      int wrote = 0;
      HandleAttr data;
      // Write to the underlying pipe.  Calling JSPipeNode::Write
      // would write using PostMessage.
      int error = PipeNode::Write(data, buffer, num_bytes, &wrote);
      if (error || wrote != num_bytes)
        return EIO;
      return 0;
    }
  }

  return EINVAL;
}

}  // namespace nacl_io
