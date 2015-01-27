// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_GDATA_WAPI_PARSER_H_
#define GOOGLE_APIS_DRIVE_GDATA_WAPI_PARSER_H_

#include <string>

#include "base/macros.h"

// Defines data elements of Google Documents API as described in
// http://code.google.com/apis/documents/.
namespace google_apis {

// This class represents a resource entry. A resource is a generic term which
// refers to a file and a directory.
class ResourceEntry {
 public:
  enum ResourceEntryKind {
    ENTRY_KIND_UNKNOWN,
    ENTRY_KIND_FOLDER,
    ENTRY_KIND_FILE
  };
  ResourceEntry();
  ~ResourceEntry();

  // The resource ID is used to identify a resource, which looks like:
  // file:d41d8cd98f00b204e9800998ecf8
  const std::string& resource_id() const { return resource_id_; }

  ResourceEntryKind kind() const { return kind_; }
  const std::string& title() const { return title_; }

  // True if the file or directory is deleted (applicable to change list only).
  bool deleted() const { return deleted_; }

  // True if resource entry is a folder (collection).
  bool is_folder() const {
    return kind_ == ENTRY_KIND_FOLDER;
  }
  // True if resource entry is regular file.
  bool is_file() const {
    return kind_ == ENTRY_KIND_FILE;
  }

  void set_resource_id(const std::string& resource_id) {
    resource_id_ = resource_id;
  }
  void set_kind(ResourceEntryKind kind) { kind_ = kind; }
  void set_title(const std::string& title) { title_ = title; }
  void set_deleted(bool deleted) { deleted_ = deleted; }

 private:
  std::string resource_id_;
  ResourceEntryKind kind_;
  std::string title_;
  bool deleted_;

  DISALLOW_COPY_AND_ASSIGN(ResourceEntry);
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_GDATA_WAPI_PARSER_H_
