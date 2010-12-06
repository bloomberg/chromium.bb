// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_EXTENSION_PROVIDER_H_
#pragma once

#include "chrome/common/extensions/extension.h"

class FilePath;
class Version;

// This class is an abstract class for implementing external extensions
// providers.
class ExternalExtensionProvider {
 public:
  // ExternalExtensionProvider uses this interface to communicate back to the
  // caller what extensions are registered, and which |id|, |version| and |path|
  // they have. See also VisitRegisteredExtension below. Ownership of |version|
  // is not transferred to the visitor.
  class Visitor {
   public:
     virtual void OnExternalExtensionFileFound(
         const std::string& id,
         const Version* version,
         const FilePath& path,
         Extension::Location location) = 0;

     virtual void OnExternalExtensionUpdateUrlFound(
         const std::string& id,
         const GURL& update_url,
         Extension::Location location) = 0;

   protected:
     virtual ~Visitor() {}
  };

  virtual ~ExternalExtensionProvider() {}

  // Enumerate registered extension, calling OnExternalExtensionFound on
  // the |visitor| object for each registered extension found. |ids_to_ignore|
  // contains a list of extension ids that should not result in a call back.
  virtual void VisitRegisteredExtension(Visitor* visitor) const = 0;

  // Test if this provider has an extension with id |id| registered.
  virtual bool HasExtension(const std::string& id) const = 0;

  // Gets details of an extension by its id.  Output params will be set only
  // if they are not NULL.  If an output parameter is not specified by the
  // provider type, it will not be changed.
  // This function is no longer used outside unit tests.
  virtual bool GetExtensionDetails(const std::string& id,
                                   Extension::Location* location,
                                   scoped_ptr<Version>* version) const = 0;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_EXTENSION_PROVIDER_H_
