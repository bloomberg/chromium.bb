// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// @file file_io.cc
/// This example demonstrates the use of persistent file I/O

#include <stdio.h>

#include <sstream>
#include <string>

#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace {
/// Used for our simple protocol to communicate with Javascript
const char* const kLoadPrefix = "ld";
const char* const kSavePrefix = "sv";
const char* const kDeletePrefix = "de";
}

/// The Instance class.  One of these exists for each instance of your NaCl
/// module on the web page.  The browser will ask the Module object to create
/// a new Instance for each occurence of the <embed> tag that has these
/// attributes:
///     type="application/x-nacl"
///     src="file_io.nmf"
class FileIoInstance : public pp::Instance {
 public:
  /// The constructor creates the plugin-side instance.
  /// @param[in] instance the handle to the browser-side plugin instance.
  explicit FileIoInstance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this),
        file_system_(this, PP_FILESYSTEMTYPE_LOCALPERSISTENT),
        file_system_ready_(false) {
  }

  virtual ~FileIoInstance() {
  }

  virtual bool Init(uint32_t /*argc*/, const char* /*argn*/[],
      const char* /*argv*/[]) {
    pp::CompletionCallback callback = callback_factory_.NewCallback(
        &FileIoInstance::FileSystemOpenCallback);
    int32_t rv = file_system_.Open(1024*1024, callback);
    if (rv != PP_OK_COMPLETIONPENDING) {
      callback.Run(rv);
      return false;
    }
    return true;
  }

 protected:
  pp::CompletionCallbackFactory<FileIoInstance> callback_factory_;
  pp::FileSystem file_system_;
  bool file_system_ready_;

 private:
  /// Struct to hold various info about a file operation. Our scheme in this
  /// example is to allocate this information on the heap so that is persists
  /// after the asynchronous call until the callback is called, and is
  /// therefore available to the main thread to complete the requested operation
  struct Request {
    pp::FileRef ref;
    pp::FileIO file;
    std::string file_contents;
    int64_t offset;
    PP_FileInfo info;
  };

  /// Handler for messages coming in from the browser via postMessage().  The
  /// @a var_message can contain anything: a JSON string; a string that encodes
  /// method names and arguments; etc.
  ///
  /// Here we use messages to communicate with the user interface
  ///
  /// @param[in] var_message The message posted by the browser.
  virtual void HandleMessage(const pp::Var& var_message) {
    if (!var_message.is_string())
      return;
    
    // Parse message into: instruction file_name_length file_name [file_text]
    std::string message = var_message.AsString();
    std::string instruction;
    std::string file_name;
    std::stringstream reader(message);
    int file_name_length;

    reader >> instruction >> file_name_length;
    file_name.resize(file_name_length);
    reader.ignore(1);  // Eat the delimiter
    reader.read(&file_name[0], file_name_length);

    if (file_name.length() == 0 || file_name[0] != '/') {
      ShowStatusMessage("File name must begin with /");
      return;
    }

    // Dispatch the instruction
    if (instruction.compare(kLoadPrefix) == 0) {
      Load(file_name);
      return;
    }
    
    if (instruction.compare(kSavePrefix) == 0) {
      // Read the rest of the message as the file text
      reader.ignore(1);  // Eat the delimiter
      std::string file_text = message.substr(reader.tellg());
      Save(file_name, file_text);
      return;
    }

    if (instruction.compare(kDeletePrefix) == 0) {
      Delete(file_name);
      return;
    }
  }

  bool Save(const std::string& file_name, const std::string& file_contents) {
    if (!file_system_ready_) {
      ShowErrorMessage("File system is not open", PP_ERROR_FAILED);
      return false;
    }

    FileIoInstance::Request* request = new FileIoInstance::Request;
    request->ref = pp::FileRef(file_system_, file_name.c_str());
    request->file = pp::FileIO(this);
    request->file_contents = file_contents;

    pp::CompletionCallback callback = callback_factory_.NewCallback(
          &FileIoInstance::SaveOpenCallback, request);
    int32_t rv = request->file.Open(request->ref,
        PP_FILEOPENFLAG_WRITE|PP_FILEOPENFLAG_CREATE|PP_FILEOPENFLAG_TRUNCATE,
        callback);

    // Handle cleanup in the callback if error
    if (rv != PP_OK_COMPLETIONPENDING) {
      callback.Run(rv);
      return false;
    }
    return true;
  }

  void SaveOpenCallback(int32_t result, FileIoInstance::Request* request) {
    if (result != PP_OK) {
      ShowErrorMessage("File open for write failed", result);
      delete request;
      return;
    }

    // It is an error to write 0 bytes to the file, however,
    // upon opening we have truncated the file to length 0
    if (request->file_contents.length() == 0) {
      pp::CompletionCallback callback = callback_factory_.NewCallback(
          &FileIoInstance::SaveFlushCallback, request);
      int32_t rv = request->file.Flush(callback);

      // Handle cleanup in the callback if error
      if (rv != PP_OK_COMPLETIONPENDING) {
        callback.Run(rv);
        return;
      }
    } else if (request->file_contents.length() <= INT32_MAX) {
      request->offset = 0;
      pp::CompletionCallback callback = callback_factory_.NewCallback(
          &FileIoInstance::SaveWriteCallback, request);
        
      int32_t rv = request->file.Write(request->offset,
          request->file_contents.c_str(),
          request->file_contents.length(), callback);

      // Handle cleanup in the callback if error
      if (rv != PP_OK_COMPLETIONPENDING) {
        callback.Run(rv);
        return;
      }
    } else {
      ShowErrorMessage("File too big", PP_ERROR_FILETOOBIG);
      delete request;
      return;
    }
  }

  void SaveWriteCallback(int32_t bytes_written,
      FileIoInstance::Request* request) {
    // bytes_written is the error code if < 0
    if (bytes_written < 0) {
      ShowErrorMessage("File write failed", bytes_written);
      delete request;
      return;
    }

    // Ensure the content length is something write() can handle
    assert(request->file_contents.length() <= INT32_MAX);

    request->offset += bytes_written;

    if (request->offset == request->file_contents.length() ||
        bytes_written == 0) {
      // All bytes have been written, flush the write buffer to complete
      pp::CompletionCallback callback = callback_factory_.NewCallback(
          &FileIoInstance::SaveFlushCallback, request);
      int32_t rv = request->file.Flush(callback);

      // Handle cleanup in the callback if error
      if (rv != PP_OK_COMPLETIONPENDING) {
        callback.Run(rv);
        return;
      }
    } else {
      // If all the bytes haven't been written call write again with remainder
      pp::CompletionCallback callback = callback_factory_.NewCallback(
          &FileIoInstance::SaveWriteCallback, request);
      int32_t rv = request->file.Write(request->offset,
          request->file_contents.c_str() + request->offset,
          request->file_contents.length() - request->offset, callback);

      // Handle cleanup in the callback if error
      if (rv != PP_OK_COMPLETIONPENDING) {
        callback.Run(rv);
        return;
      }
    }
  }

  void SaveFlushCallback(int32_t result, FileIoInstance::Request* request) {
    if (result != PP_OK) {
      ShowErrorMessage("File fail to flush", result);
      delete request;
      return;
    }
    ShowStatusMessage("Save successful");
    delete request;
  }

  bool Load(const std::string& file_name) {
    if (!file_system_ready_) {
      ShowErrorMessage("File system is not open", PP_ERROR_FAILED);
      return false;
    }

    FileIoInstance::Request* request = new FileIoInstance::Request;
    request->ref = pp::FileRef(file_system_, file_name.c_str());
    request->file = pp::FileIO(this);

    pp::CompletionCallback callback = callback_factory_.NewCallback(
          &FileIoInstance::LoadOpenCallback, request);
    int32_t rv = request->file.Open(request->ref, PP_FILEOPENFLAG_READ,
        callback);

    // Handle cleanup in the callback if error
    if (rv != PP_OK_COMPLETIONPENDING) {
      callback.Run(rv);
      return false;
    }
    return true;
  }

  void LoadOpenCallback(int32_t result, FileIoInstance::Request* request) {
    if (result == PP_ERROR_FILENOTFOUND) {
      ShowStatusMessage("File not found");
      delete request;
      return;
    } else if (result != PP_OK) {
      ShowErrorMessage("File open for read failed", result);
      delete request;
      return;
    }

    pp::CompletionCallback callback = callback_factory_.NewCallback(
          &FileIoInstance::LoadQueryCallback, request);
    int32_t rv = request->file.Query(&request->info, callback);

    // Handle cleanup in the callback if error
    if (rv != PP_OK_COMPLETIONPENDING) {
      callback.Run(rv);
      return;
    }
  }

  void LoadQueryCallback(int32_t result, FileIoInstance::Request* request) {
    if (result != PP_OK) {
      ShowErrorMessage("File query failed", result);
      delete request;
      return;
    }

    // FileIO.Read() can only handle int32 sizes
    if (request->info.size > INT32_MAX) {
      ShowErrorMessage("File too big", PP_ERROR_FILETOOBIG);
      delete request;
      return;
    }

    // Allocate a buffer to read the file into
    // Here we must allocate on the heap so FileIO::Read will write to this
    // one and only copy of file_contents
    request->file_contents.resize(request->info.size, '\0');
    request->offset = 0;

    pp::CompletionCallback callback = callback_factory_.NewCallback(
        &FileIoInstance::LoadReadCallback, request);
    int32_t rv = request->file.Read(request->offset,
        &request->file_contents[request->offset],
        request->file_contents.length(), callback);

    // Handle cleanup in the callback if error
    if (rv != PP_OK_COMPLETIONPENDING) {
      callback.Run(rv);
      return;
    }
  }

  void LoadReadCallback(int32_t bytes_read, FileIoInstance::Request* request) {
    // If bytes_read < 0 then it indicates the error code
    if (bytes_read < 0) {
      ShowErrorMessage("File read failed", bytes_read);
      delete request;
      return;
    }

    // Ensure the content length is something read() can handle
    assert(request->file_contents.length() <= INT32_MAX);

    request->offset += bytes_read;

    if (request->offset == request->file_contents.length() || bytes_read == 0) {
      // Done reading, send content to the user interface
      PostMessage(pp::Var("DISP|" + request->file_contents));
      ShowStatusMessage("Load complete");
      delete request;
    } else {
      // Some bytes remain to be read, call read again with remainder
      pp::CompletionCallback callback = callback_factory_.NewCallback(
            &FileIoInstance::LoadReadCallback, request);
      int32_t rv = request->file.Read(request->offset,
          &request->file_contents[request->offset],
          request->file_contents.length() - request->offset, callback);

      // Handle cleanup in the callback if error
      if (rv != PP_OK_COMPLETIONPENDING) {
        callback.Run(rv);
        return;
      }
    }
  }

  bool Delete(const std::string& file_name) {
    if (!file_system_ready_) {
      ShowErrorMessage("File system is not open", PP_ERROR_FAILED);
      return false;
    }

    FileIoInstance::Request* request = new FileIoInstance::Request;
    request->ref = pp::FileRef(file_system_, file_name.c_str());
   
    pp::CompletionCallback callback = callback_factory_.NewCallback(
        &FileIoInstance::DeleteCallback, request);
    int32_t rv = request->ref.Delete(callback);

    // Handle cleanup in the callback if error
    if (rv != PP_OK_COMPLETIONPENDING) {
      callback.Run(rv);
      return false;
    }
    return true;
  }

  void DeleteCallback(int32_t result, FileIoInstance::Request* request) {
    if (result == PP_ERROR_FILENOTFOUND) {
      ShowStatusMessage("File not found");
      delete request;
      return;
    } else if (result != PP_OK) {
      ShowErrorMessage("Deletion failed", result);
      delete request;
      return;
    }

    ShowStatusMessage("File deleted");
    delete request;
  }

  void FileSystemOpenCallback(int32_t result) {
    if (result != PP_OK) {
      ShowErrorMessage("File system open call failed", result);
      return;
    }

    file_system_ready_ = true;
    // Notify the user interface that we're ready
    PostMessage(pp::Var("READY|"));
  }

  /// Encapsulates our simple javascript communication protocol
  void ShowErrorMessage(const std::string& message, int32_t result) {
    std::stringstream ss;
    ss << "ERR|" << message << " -- Error #: " << result;
    PostMessage(pp::Var(ss.str()));
  }

  /// Encapsulates our simple javascript communication protocol
  void ShowStatusMessage(const std::string& message) {
    std::stringstream ss;
    ss << "STAT|" << message;
    PostMessage(pp::Var(ss.str()));
  }
};

/// The Module class.  The browser calls the CreateInstance() method to create
/// an instance of your NaCl module on the web page.  The browser creates a new
/// instance for each <embed> tag with type="application/x-nacl".
class FileIoModule : public pp::Module {
 public:
  FileIoModule() : pp::Module() {}
  virtual ~FileIoModule() {}

  /// Create and return a FileIoInstance object.
  /// @param[in] instance The browser-side instance.
  /// @return the plugin-side instance.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new FileIoInstance(instance);
  }
};

namespace pp {
/// Factory function called by the browser when the module is first loaded.
/// The browser keeps a singleton of this module.  It calls the
/// CreateInstance() method on the object you return to make instances.  There
/// is one instance per <embed> tag on the page.  This is the main binding
/// point for your NaCl module with the browser.
Module* CreateModule() {
  return new FileIoModule();
}
}  // namespace pp

