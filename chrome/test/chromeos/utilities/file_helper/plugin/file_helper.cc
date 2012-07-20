// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <string>
#include <vector>

#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

namespace {
  size_t Tokenize(const std::string& str,
                  const std::string& delimiters,
                  std::vector<std::string>* tokens) {
    tokens->clear();

    std::string::size_type start = str.find_first_not_of(delimiters);
    while (start != std::string::npos) {
      std::string::size_type end = str.find_first_of(delimiters, start + 1);
      if (end == std::string::npos) {
        tokens->push_back(str.substr(start));
        break;
      } else {
        tokens->push_back(str.substr(start, end - start));
        start = str.find_first_not_of(delimiters, end + 1);
      }
    }

    return tokens->size();
  }
}

namespace file_reader {
// Method name for file read command, as seen by JavaScript code.
const char kReadMethodId[] = "readFile";

// Method name for file write command, as seen by JavaScript code.
const char kWriteMethodId[] = "writeFile";

// Method name for file write command, as seen by JavaScript code.
const char kCreateMethodId[] = "createFile";

// Method name for file truncate command, as seen by JavaScript code.
const char kTruncateMethodId[] = "truncateFile";

// Separator character for the reverseText method.
const char kMessageArgumentSeparator[] = ":";

class FileHelper {
 public:
  enum FileOperation {
    OPERATION_UNKNOWN,
    OPERATION_READ,
    OPERATION_WRITE,
    OPERATION_CREATE,
    OPERATION_TRUNCATE
  };

  FileHelper(pp::Instance* instance,
             const std::string& file_path)
      : path_(file_path),
        instance_(instance),
        file_system_(NULL),
        file_ref_(NULL),
        file_io_(NULL),
        operation_(OPERATION_UNKNOWN) {
  }

  ~FileHelper() {
    if (file_io_)
      delete file_io_;
    if (file_ref_)
      delete file_ref_;
    if (file_system_)
      delete file_system_;
  }

  void TruncateFile() {
    operation_ = OPERATION_TRUNCATE;
    OpenFileSystem();
  }

  void ReadFromFile() {
    operation_ = OPERATION_READ;
    OpenFileSystem();
  }

  void WriteToFile(const std::string& data) {
    operation_ = OPERATION_WRITE;
    data_ = data;
    OpenFileSystem();
  }

  void CreateFile(const std::string& data) {
    operation_ = OPERATION_CREATE;
    data_ = data;
    OpenFileSystem();
  }

 private:

  struct ReadInfo {
    char buf[256];
    int32_t offset;
    FileHelper* self;
    std::string data;
  };

  void ReportError(const char* msg) {
    instance_->PostMessage(pp::Var(msg));
    delete this;
  }

  int GetFileOpenFlags() {
    switch (operation_) {
      case OPERATION_READ:
        return PP_FILEOPENFLAG_READ;
      case OPERATION_WRITE:
        return static_cast<int>(PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_READ);
      case OPERATION_CREATE:
        return static_cast<int>(PP_FILEOPENFLAG_CREATE |
                                PP_FILEOPENFLAG_WRITE |
                                PP_FILEOPENFLAG_READ |
                                PP_FILEOPENFLAG_EXCLUSIVE);
      case OPERATION_TRUNCATE:
        return PP_FILEOPENFLAG_TRUNCATE | PP_FILEOPENFLAG_WRITE;
      case OPERATION_UNKNOWN:
        break;
    }
    return PP_FILEOPENFLAG_READ;
  }

  void OpenFileSystem() {
    file_system_ = new pp::FileSystem(instance_, PP_FILESYSTEMTYPE_EXTERNAL);
    file_system_->Open(4*1024*1024,
                       pp::CompletionCallback(&OnFileSystemOpen, this));
  }

  void OpenFile() {
    file_io_ = new pp::FileIO(instance_);
    file_ref_ = new pp::FileRef(*file_system_, path_.c_str());
    file_io_->Open(*file_ref_, GetFileOpenFlags(),
                   pp::CompletionCallback(&OnFileOpen, this));
  }

  void ProcessFile() {
    switch (operation_) {
      case OPERATION_READ:
        ProcessRead();
        break;
      case OPERATION_WRITE:
        ProcessWrite();
        break;
      case OPERATION_CREATE:
        ProcessWrite();
        break;
      case OPERATION_TRUNCATE:
        ProcessTruncate();
        break;
      case OPERATION_UNKNOWN:
        CompleteOperation(NULL, "FAIL");
        break;
    }
  }

  void ProcessWrite() {
    ReadInfo* info = new ReadInfo();
    info->self = this;
    info->data = data_;
    info->offset = 0;
    WriteChunk(info);
  }

  void WriteChunk(ReadInfo* info) {
    file_io_->Write(info->offset,
                    info->data.c_str() + info->offset,
                    info->data.length() - info->offset,
                    pp::CompletionCallback(&WriteChunkCallback, info));

  }

  void ProcessRead() {
    ReadInfo* info = new ReadInfo();
    info->self = this;
    info->offset = 0;
    ReadChunk(info);
  }

  void ReadChunk(ReadInfo* info) {
    file_io_->Read(info->offset, info->buf, sizeof(info->buf),
                   pp::CompletionCallback(&ReadChunkCallback, info));
  }

  void ProcessTruncate() {
    CompleteOperation(NULL, "OK");
  }

  void CompleteOperation(ReadInfo* info, const char* msg) {
    instance_->PostMessage(pp::Var(msg));
    file_io_->Close();
    if (info)
      delete info;
    delete this;
  }

  static void WriteChunkCallback(void* user_data, int32_t result) {
    ReadInfo* info = reinterpret_cast<ReadInfo*>(user_data);
    FileHelper* self = reinterpret_cast<FileHelper*>(info->self);
    if (result == 0) {
      self->CompleteOperation(info, "OK");
    } else if (result < 0) {
      self->CompleteOperation(info, "FAIL");
    } else {
      info->offset += result;
      self->WriteChunk(info);
    }
  }

  static void ReadChunkCallback(void* user_data, int32_t result) {
    ReadInfo* info = reinterpret_cast<ReadInfo*>(user_data);
    FileHelper* self = reinterpret_cast<FileHelper*>(info->self);
    if (result == 0) {
      self->CompleteOperation(info, info->data.c_str());
    } else if (result < 0) {
      self->CompleteOperation(info, "FAIL");
    } else {
      info->data.append(info->buf, result);
      info->offset += result;
      self->ReadChunk(info);
    }
  }

  static void OnFileSystemOpen(void* user_data, int32_t result) {
    FileHelper* self = reinterpret_cast<FileHelper*>(user_data);
    if (result != PP_OK) {
      self->ReportError("Failed to open file system.");
      return;
    }

    self->OpenFile();
  }

  static void OnFileOpen(void* user_data, int32_t result) {
    FileHelper* self = reinterpret_cast<FileHelper*>(user_data);
    if (result != PP_OK) {
      self->ReportError("Failed to open file.");
      return;
    }
    self->ProcessFile();
  }

 private:
  std::string path_;
  std::string data_;
  pp::Instance* instance_;
  pp::FileSystem* file_system_;
  pp::FileRef* file_ref_;
  pp::FileIO* file_io_;
  FileOperation operation_;
};

class FileHelperInstance : public pp::Instance {
 public:
  explicit FileHelperInstance(PP_Instance instance)
      : pp::Instance(instance) {}
  virtual ~FileHelperInstance() {}

  virtual void HandleMessage(const pp::Var& var_message);

};

void FileHelperInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_string()) {
    return;
  }
  std::string message = var_message.AsString();
  std::vector<std::string> args;
  pp::Var return_var;
  Tokenize(message, std::string(kMessageArgumentSeparator), &args);
  if (args.size() < 2) {
    PostMessage(return_var);
    return;
  }
  const std::string& function = args[0];
  const std::string& file_path = args[1];
  FileHelper* helper = new FileHelper(this, file_path);
  if (function == kReadMethodId) {
    helper->ReadFromFile();
    return;
  } else if (function == kTruncateMethodId) {
    helper->TruncateFile();
    return;
  } else if (function == kWriteMethodId) {
    if (args.size() < 3) {
      PostMessage(return_var);
      return;
    }
    const std::string& data = args[2];
    helper->WriteToFile(data);
    return;
  } else if (function == kCreateMethodId) {
    if (args.size() < 3) {
      PostMessage(return_var);
      return;
    }
    const std::string& data = args[2];
    helper->CreateFile(data);
    return;
  }
  PostMessage(return_var);
}

class FileHelperModule : public pp::Module {
 public:
  FileHelperModule() : pp::Module() {}
  virtual ~FileHelperModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new FileHelperInstance(instance);
  }
};
}  // namespace file_reader


namespace pp {
Module* CreateModule() {
  return new file_reader::FileHelperModule();
}
}  // namespace pp
