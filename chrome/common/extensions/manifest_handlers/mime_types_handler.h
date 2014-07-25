// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_MIME_TYPES_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_MIME_TYPES_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

class MimeTypesHandler {
 public:
  // Returns list of extensions' ids that are allowed to use MIME type filters.
  static std::vector<std::string> GetMIMETypeWhitelist();

  static MimeTypesHandler* GetHandler(const extensions::Extension* extension);

  MimeTypesHandler();
  ~MimeTypesHandler();

  // extension id
  std::string extension_id() const { return extension_id_; }
  void set_extension_id(const std::string& extension_id) {
    extension_id_ = extension_id;
  }

  // Adds a MIME type filter to the handler.
  void AddMIMEType(const std::string& mime_type);
  // Tests if the handler has registered a filter for the MIME type.
  bool CanHandleMIMEType(const std::string& mime_type) const;

  // Set the URL that will be used to handle MIME type requests.
  void set_handler_url(const std::string& handler_url) {
    handler_url_ = handler_url;
  }
  // The URL that will be used to handle MIME type requests.
  const std::string& handler_url() const { return handler_url_; }

  const std::set<std::string>& mime_type_set() const { return mime_type_set_; }

 private:
  // The id for the extension this action belongs to (as defined in the
  // extension manifest).
  std::string extension_id_;

  // A list of MIME type filters.
  std::set<std::string> mime_type_set_;

  std::string handler_url_;
};

class MimeTypesHandlerParser : public extensions::ManifestHandler {
 public:
  MimeTypesHandlerParser();
  virtual ~MimeTypesHandlerParser();

  virtual bool Parse(extensions::Extension* extension,
                     base::string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;
};

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_MIME_TYPES_HANDLER_H_
