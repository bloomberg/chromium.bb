// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// @file file_io.cc
/// This example demonstrates the use of persistent file I/O

#define __STDC_LIMIT_MACROS
#include <sstream>
#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/threading/simple_thread.h"

#ifndef INT32_MAX
#define INT32_MAX (0x7FFFFFFF)
#endif

#ifdef WIN32
#undef min
#undef max
#undef PostMessage

// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

namespace {
/// Used for our simple protocol to communicate with Javascript
const char* const kLoadPrefix = "ld";
const char* const kSavePrefix = "sv";
const char* const kDeletePrefix = "de";
}

/// The Instance class.  One of these exists for each instance of your NaCl
/// module on the web page.  The browser will ask the Module object to create
/// a new Instance for each occurrence of the <embed> tag that has these
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
        file_system_ready_(false),
        file_thread_(this) {
  }

  virtual ~FileIoInstance() {
    file_thread_.Join();
  }

  virtual bool Init(uint32_t /*argc*/, const char* /*argn*/[],
      const char* /*argv*/[]) {
    file_thread_.Start();
    // Open the file system on the file_thread_. Since this is the first
    // operation we perform there, and because we do everything on the
    // file_thread_ synchronously, this ensures that the FileSystem is open
    // before any FileIO operations execute.
    file_thread_.message_loop().PostWork(
        callback_factory_.NewCallback(&FileIoInstance::OpenFileSystem));
    return true;
  }

 private:
  pp::CompletionCallbackFactory<FileIoInstance> callback_factory_;
  pp::FileSystem file_system_;

  // Indicates whether file_system_ was opened successfully. We only read/write
  // this on the file_thread_.
  bool file_system_ready_;

  // We do all our file operations on the file_thread_.
  pp::SimpleThread file_thread_;

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
      file_thread_.message_loop().PostWork(
          callback_factory_.NewCallback(&FileIoInstance::Load, file_name));
      return;
    }

    if (instruction.compare(kSavePrefix) == 0) {
      // Read the rest of the message as the file text
      reader.ignore(1);  // Eat the delimiter
      std::string file_text = message.substr(reader.tellg());
      file_thread_.message_loop().PostWork(
          callback_factory_.NewCallback(&FileIoInstance::Save,
                                        file_name,
                                        file_text));
      return;
    }

    if (instruction.compare(kDeletePrefix) == 0) {
      file_thread_.message_loop().PostWork(
          callback_factory_.NewCallback(&FileIoInstance::Delete, file_name));
      return;
    }
  }

  void OpenFileSystem(int32_t /* result */) {
    int32_t rv = file_system_.Open(1024*1024, pp::CompletionCallback());
    if (rv == PP_OK) {
      file_system_ready_ = true;
      // Notify the user interface that we're ready
      PostMessage(pp::Var("READY|"));
    } else {
      ShowErrorMessage("Failed to open file system", rv);
    }
  }

  void Save(int32_t /* result */, const std::string& file_name,
            const std::string& file_contents) {
    if (!file_system_ready_) {
      ShowErrorMessage("File system is not open", PP_ERROR_FAILED);
      return;
    }
    pp::FileRef ref(file_system_, file_name.c_str());
    pp::FileIO file(this);

    int32_t open_result = file.Open(
        ref,
        PP_FILEOPENFLAG_WRITE|PP_FILEOPENFLAG_CREATE|PP_FILEOPENFLAG_TRUNCATE,
        pp::CompletionCallback());
    if (open_result != PP_OK) {
      ShowErrorMessage("File open for write failed", open_result);
      return;
    }

    // We have truncated the file to 0 bytes. So we need only write if
    // file_contents is non-empty.
    if (!file_contents.empty()) {
      if (file_contents.length() > INT32_MAX) {
        ShowErrorMessage("File too big", PP_ERROR_FILETOOBIG);
        return;
      }
      int64_t offset = 0;
      int32_t bytes_written = 0;
      do {
        bytes_written = file.Write(offset,
                                   file_contents.data() + offset,
                                   file_contents.length(),
                                   pp::CompletionCallback());
        if (bytes_written > 0) {
          offset += bytes_written;
        } else {
          ShowErrorMessage("File write failed", bytes_written);
          return;
        }
      } while (bytes_written < static_cast<int64_t>(file_contents.length()));
    }
    // All bytes have been written, flush the write buffer to complete
    int32_t flush_result = file.Flush(pp::CompletionCallback());
    if (flush_result != PP_OK) {
      ShowErrorMessage("File fail to flush", flush_result);
      return;
    }
    ShowStatusMessage("Save successful");
  }

  void Load(int32_t /* result */, const std::string& file_name) {
    if (!file_system_ready_) {
      ShowErrorMessage("File system is not open", PP_ERROR_FAILED);
      return;
    }
    pp::FileRef ref(file_system_, file_name.c_str());
    pp::FileIO file(this);

    int32_t open_result = file.Open(ref, PP_FILEOPENFLAG_READ,
                                    pp::CompletionCallback());
    if (open_result == PP_ERROR_FILENOTFOUND) {
      ShowStatusMessage("File not found");
      return;
    } else if (open_result != PP_OK) {
      ShowErrorMessage("File open for read failed", open_result);
      return;
    }
    PP_FileInfo info;
    int32_t query_result = file.Query(&info, pp::CompletionCallback());
    if (query_result != PP_OK) {
      ShowErrorMessage("File query failed", query_result);
      return;
    }
    // FileIO.Read() can only handle int32 sizes
    if (info.size > INT32_MAX) {
      ShowErrorMessage("File too big", PP_ERROR_FILETOOBIG);
      return;
    }

    std::vector<char> data(info.size);
    int64_t offset = 0;
    int32_t bytes_read = 0;
    do {
      bytes_read = file.Read(offset, &data[offset], data.size() - offset,
                             pp::CompletionCallback());
      if (bytes_read > 0)
        offset += bytes_read;
    } while (bytes_read > 0);
    // If bytes_read < PP_OK then it indicates the error code.
    if (bytes_read < PP_OK) {
      ShowErrorMessage("File read failed", bytes_read);
      return;
    }
    PP_DCHECK(bytes_read == 0);
    // Done reading, send content to the user interface
    std::string string_data(data.begin(), data.end());
    PostMessage(pp::Var("DISP|" + string_data));
    ShowStatusMessage("Load complete");
  }

  void Delete(int32_t /* result */, const std::string& file_name) {
    if (!file_system_ready_) {
      ShowErrorMessage("File system is not open", PP_ERROR_FAILED);
      return;
    }
    pp::FileRef ref(file_system_, file_name.c_str());

    int32_t result = ref.Delete(pp::CompletionCallback());
    if (result == PP_ERROR_FILENOTFOUND) {
      ShowStatusMessage("File not found");
      return;
    } else if (result != PP_OK) {
      ShowErrorMessage("Deletion failed", result);
      return;
    }
    ShowStatusMessage("File deleted");
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

