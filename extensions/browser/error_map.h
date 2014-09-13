// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ERROR_MAP_H_
#define EXTENSIONS_BROWSER_ERROR_MAP_H_

#include <deque>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/extension_error.h"

namespace extensions {

typedef std::deque<const ExtensionError*> ErrorList;

// An ErrorMap is responsible for storing Extension-related errors, keyed by
// Extension ID. The errors are owned by the ErrorMap, and are deleted upon
// destruction.
class ErrorMap {
 public:
  ErrorMap();
  ~ErrorMap();

  // Return the list of all errors associated with the given extension.
  const ErrorList& GetErrorsForExtension(const std::string& extension_id) const;

  // Add the |error| to the ErrorMap.
  const ExtensionError* AddError(scoped_ptr<ExtensionError> error);

  // Remove an extension from the ErrorMap, deleting all associated errors.
  void Remove(const std::string& extension_id);
  // Remove all errors of a given type for an extension.
  void RemoveErrorsForExtensionOfType(const std::string& extension_id,
                                      ExtensionError::Type type);
  // Remove all incognito errors for all extensions.
  void RemoveIncognitoErrors();
  // Remove all errors for all extensions, and clear the map.
  void RemoveAllErrors();

  size_t size() const { return map_.size(); }

 private:
  // An Entry is created for each Extension ID, and stores the errors related to
  // that Extension.
  class ExtensionEntry {
   public:
    ExtensionEntry();
    ~ExtensionEntry();

    // Delete all errors associated with this extension.
    void DeleteAllErrors();
    // Delete all incognito errors associated with this extension.
    void DeleteIncognitoErrors();
    // Delete all errors of the given |type| associated with this extension.
    void DeleteErrorsOfType(ExtensionError::Type type);

    // Add the error to the list, and return a weak reference.
    const ExtensionError* AddError(scoped_ptr<ExtensionError> error);

    const ErrorList* list() const { return &list_; }

   private:
    // The list of all errors associated with the extension. The errors are
    // owned by the Entry (in turn owned by the ErrorMap) and are deleted upon
    // destruction.
    ErrorList list_;

    DISALLOW_COPY_AND_ASSIGN(ExtensionEntry);
  };
  typedef std::map<std::string, ExtensionEntry*> EntryMap;

  // The mapping between Extension IDs and their corresponding Entries.
  EntryMap map_;

  DISALLOW_COPY_AND_ASSIGN(ErrorMap);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ERROR_MAP_H_
