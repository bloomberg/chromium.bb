// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WARNING_SET_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WARNING_SET_H_

#include <set>
#include <string>
#include <vector>

#include "url/gurl.h"

// TODO(battre) Remove the Extension prefix.

namespace base {
class FilePath;
}

namespace extensions {

class ExtensionSet;

// This class is used by the ExtensionWarningService to represent warnings if
// extensions misbehave. Note that the ExtensionWarningService deals only with
// specific warnings that should trigger a badge on the Chrome menu button.
class ExtensionWarning {
 public:
  enum WarningType {
    // Don't use this, it is only intended for the default constructor and
    // does not have localized warning messages for the UI.
    kInvalid = 0,
    // An extension caused excessive network delays.
    kNetworkDelay,
    // This extension failed to modify a network request because the
    // modification conflicted with a modification of another extension.
    kNetworkConflict,
    // This extension failed to redirect a network request because another
    // extension with higher precedence redirected to a different target.
    kRedirectConflict,
    // The extension repeatedly flushed WebKit's in-memory cache, which slows
    // down the overall performance.
    kRepeatedCacheFlushes,
    // The extension failed to determine the filename of a download because
    // another extension with higher precedence determined a different filename.
    kDownloadFilenameConflict,
    kReloadTooFrequent,
    kMaxWarningType
  };

  // We allow copy&assign for passing containers of ExtensionWarnings between
  // threads.
  ExtensionWarning(const ExtensionWarning& other);
  ~ExtensionWarning();
  ExtensionWarning& operator=(const ExtensionWarning& other);

  // Factory methods for various warning types.
  static ExtensionWarning CreateNetworkDelayWarning(
      const std::string& extension_id);
  static ExtensionWarning CreateNetworkConflictWarning(
      const std::string& extension_id);
  static ExtensionWarning CreateRedirectConflictWarning(
      const std::string& extension_id,
      const std::string& winning_extension_id,
      const GURL& attempted_redirect_url,
      const GURL& winning_redirect_url);
  static ExtensionWarning CreateRequestHeaderConflictWarning(
      const std::string& extension_id,
      const std::string& winning_extension_id,
      const std::string& conflicting_header);
  static ExtensionWarning CreateResponseHeaderConflictWarning(
      const std::string& extension_id,
      const std::string& winning_extension_id,
      const std::string& conflicting_header);
  static ExtensionWarning CreateCredentialsConflictWarning(
      const std::string& extension_id,
      const std::string& winning_extension_id);
  static ExtensionWarning CreateRepeatedCacheFlushesWarning(
      const std::string& extension_id);
  static ExtensionWarning CreateDownloadFilenameConflictWarning(
      const std::string& losing_extension_id,
      const std::string& winning_extension_id,
      const base::FilePath& losing_filename,
      const base::FilePath& winning_filename);
  static ExtensionWarning CreateReloadTooFrequentWarning(
      const std::string& extension_id);

  // Returns the specific warning type.
  WarningType warning_type() const { return type_; }

  // Returns the id of the extension for which this warning is valid.
  const std::string& extension_id() const { return extension_id_; }

  // Returns a localized warning message.
  std::string GetLocalizedMessage(const ExtensionSet* extensions) const;

 private:
  // Constructs a warning of type |type| for extension |extension_id|. This
  // could indicate for example the fact that an extension conflicted with
  // others. The |message_id| refers to an IDS_ string ID. The
  // |message_parameters| are filled into the message template.
  ExtensionWarning(WarningType type,
                   const std::string& extension_id,
                   int message_id,
                   const std::vector<std::string>& message_parameters);

  WarningType type_;
  std::string extension_id_;
  // IDS_* resource ID.
  int message_id_;
  // Parameters to be filled into the string identified by |message_id_|.
  std::vector<std::string> message_parameters_;
};

// Compare ExtensionWarnings based on the tuple of (extension_id, type).
// The message associated with ExtensionWarnings is purely informational
// and does not contribute to distinguishing extensions.
bool operator<(const ExtensionWarning& a, const ExtensionWarning& b);

typedef std::set<ExtensionWarning> ExtensionWarningSet;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WARNING_SET_H_
