// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_ERROR_H_
#define EXTENSIONS_BROWSER_EXTENSION_ERROR_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "extensions/common/stack_frame.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace extensions {

class ExtensionError {
 public:
  enum Type {
    MANIFEST_ERROR,
    RUNTIME_ERROR,
    NUM_ERROR_TYPES  // Put new values above this.
  };

  virtual ~ExtensionError();

  // Serializes the ExtensionError into JSON format.
  virtual scoped_ptr<base::DictionaryValue> ToValue() const;

  virtual std::string PrintForTest() const;

  // Return true if this error and |rhs| are considered equal, and should be
  // grouped together.
  bool IsEqual(const ExtensionError* rhs) const;

  Type type() const { return type_; }
  const std::string& extension_id() const { return extension_id_; }
  bool from_incognito() const { return from_incognito_; }
  logging::LogSeverity level() const { return level_; }
  const base::string16& source() const { return source_; }
  const base::string16& message() const { return message_; }
  size_t occurrences() const { return occurrences_; }
  void set_occurrences(size_t occurrences) { occurrences_ = occurrences; }

  // Keys used for retrieving JSON values.
  static const char kExtensionIdKey[];
  static const char kFromIncognitoKey[];
  static const char kLevelKey[];
  static const char kMessageKey[];
  static const char kSourceKey[];
  static const char kTypeKey[];

 protected:
  ExtensionError(Type type,
                 const std::string& extension_id,
                 bool from_incognito,
                 logging::LogSeverity level,
                 const base::string16& source,
                 const base::string16& message);

  virtual bool IsEqualImpl(const ExtensionError* rhs) const = 0;

  // Which type of error this is.
  Type type_;
  // The ID of the extension which caused the error.
  std::string extension_id_;
  // Whether or not the error was caused while incognito.
  bool from_incognito_;
  // The severity level of the error.
  logging::LogSeverity level_;
  // The source for the error; this can be a script, web page, or manifest file.
  // This is stored as a string (rather than a url) since it can be a Chrome
  // script file (e.g., event_bindings.js).
  base::string16 source_;
  // The error message itself.
  base::string16 message_;
  // The number of times this error has occurred.
  size_t occurrences_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionError);
};

class ManifestError : public ExtensionError {
 public:
  ManifestError(const std::string& extension_id,
                const base::string16& message,
                const base::string16& manifest_key,
                const base::string16& manifest_specific);
  virtual ~ManifestError();

  virtual scoped_ptr<base::DictionaryValue> ToValue() const OVERRIDE;

  virtual std::string PrintForTest() const OVERRIDE;

  const base::string16& manifest_key() const { return manifest_key_; }
  const base::string16& manifest_specific() const { return manifest_specific_; }

  // Keys used for retrieving JSON values.
  static const char kManifestKeyKey[];
  static const char kManifestSpecificKey[];

 private:
  virtual bool IsEqualImpl(const ExtensionError* rhs) const OVERRIDE;

  // If present, this indicates the feature in the manifest which caused the
  // error.
  base::string16 manifest_key_;
  // If present, this is a more-specific location of the error - for instance,
  // a specific permission which is incorrect, rather than simply "permissions".
  base::string16 manifest_specific_;

  DISALLOW_COPY_AND_ASSIGN(ManifestError);
};

class RuntimeError : public ExtensionError {
 public:
  RuntimeError(const std::string& extension_id,  // optional, sometimes unknown.
               bool from_incognito,
               const base::string16& source,
               const base::string16& message,
               const StackTrace& stack_trace,
               const GURL& context_url,
               logging::LogSeverity level,
               int render_view_id,
               int render_process_id);
  virtual ~RuntimeError();

  virtual scoped_ptr<base::DictionaryValue> ToValue() const OVERRIDE;

  virtual std::string PrintForTest() const OVERRIDE;

  const GURL& context_url() const { return context_url_; }
  const StackTrace& stack_trace() const { return stack_trace_; }
  int render_view_id() const { return render_view_id_; }
  int render_process_id() const { return render_process_id_; }

  // Keys used for retrieving JSON values.
  static const char kColumnNumberKey[];
  static const char kContextUrlKey[];
  static const char kFunctionNameKey[];
  static const char kLineNumberKey[];
  static const char kStackTraceKey[];
  static const char kUrlKey[];
  static const char kRenderProcessIdKey[];
  static const char kRenderViewIdKey[];

 private:
  virtual bool IsEqualImpl(const ExtensionError* rhs) const OVERRIDE;

  // Since we piggy-back onto other error reporting systems (like V8 and
  // WebKit), the reported information may need to be cleaned up in order to be
  // in a consistent format.
  void CleanUpInit();

  GURL context_url_;
  StackTrace stack_trace_;

  // Keep track of the render process which caused the error in order to
  // inspect the view later, if possible.
  int render_view_id_;
  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(RuntimeError);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_ERROR_H_
