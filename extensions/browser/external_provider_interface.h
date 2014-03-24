// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTERNAL_PROVIDER_INTERFACE_H_
#define EXTENSIONS_BROWSER_EXTERNAL_PROVIDER_INTERFACE_H_

#include <vector>

#include "base/memory/linked_ptr.h"
#include "extensions/common/manifest.h"

class GURL;

namespace base {
class FilePath;
class Version;
}

namespace extensions {

// This class is an abstract class for implementing external extensions
// providers.
class ExternalProviderInterface {
 public:
  // ExternalProvider uses this interface to communicate back to the
  // caller what extensions are registered, and which |id|, |version| and |path|
  // they have. See also VisitRegisteredExtension below. Ownership of |version|
  // is not transferred to the visitor.  Callers of the methods below must
  // ensure that |id| is a valid extension id (use Extension::IdIsValid(id)).
  class VisitorInterface {
   public:
    // Return true if the extension install will proceed.  Install will not
    // proceed if the extension is already installed from a higher priority
    // location.
    virtual bool OnExternalExtensionFileFound(
        const std::string& id,
        const base::Version* version,
        const base::FilePath& path,
        Manifest::Location location,
        int creation_flags,
        bool mark_acknowledged) = 0;

    // Return true if the extension install will proceed.  Install might not
    // proceed if the extension is already installed from a higher priority
    // location.
    virtual bool OnExternalExtensionUpdateUrlFound(
        const std::string& id,
        const std::string& install_parameter,
        const GURL& update_url,
        Manifest::Location location,
        int creation_flags,
        bool mark_acknowledged) = 0;

    // Called after all the external extensions have been reported
    // through the above two methods. |provider| is a pointer to the
    // provider that is now ready (typically this), and the
    // implementation of OnExternalProviderReady() should be able to
    // safely assert that provider->IsReady().
    virtual void OnExternalProviderReady(
        const ExternalProviderInterface* provider) = 0;

   protected:
    virtual ~VisitorInterface() {}
  };

  virtual ~ExternalProviderInterface() {}

  // The visitor (ExtensionsService) calls this function before it goes away.
  virtual void ServiceShutdown() = 0;

  // Enumerate registered extensions, calling
  // OnExternalExtension(File|UpdateUrl)Found on the |visitor| object for each
  // registered extension found.
  virtual void VisitRegisteredExtension() = 0;

  // Test if this provider has an extension with id |id| registered.
  virtual bool HasExtension(const std::string& id) const = 0;

  // Gets details of an extension by its id.  Output params will be set only
  // if they are not NULL.  If an output parameter is not specified by the
  // provider type, it will not be changed.
  // This function is no longer used outside unit tests.
  virtual bool GetExtensionDetails(
      const std::string& id,
      Manifest::Location* location,
      scoped_ptr<base::Version>* version) const = 0;

  // Determines if this provider had loaded the list of external extensions
  // from its source.
  virtual bool IsReady() const = 0;
};

typedef std::vector<linked_ptr<ExternalProviderInterface> >
    ProviderCollection;

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTERNAL_PROVIDER_INTERFACE_H_
