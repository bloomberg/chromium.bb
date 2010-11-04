// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_STATEFUL_EXTERNAL_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_STATEFUL_EXTERNAL_EXTENSION_PROVIDER_H_
#pragma once

#include <set>
#include <string>

#include "chrome/browser/extensions/external_extension_provider.h"

class DictionaryValue;
class ValueSerializer;
class Version;

// A specialization of the ExternalExtensionProvider that stores the registered
// external extensions in a dictionary. This dictionary (|prefs_|) will be used
// by HasExtension() and GetExtensionDetails() to return information about the
// stored external extensions. It is the responsibility of specialized
// subclasses to initialize this internal dictionary.
// This provider can provide external extensions from two sources: crx files
// and udpate URLs. The locations that the provider will report for these
// are specified at the constructor.
class StatefulExternalExtensionProvider : public ExternalExtensionProvider {
 public:
  // Initialize the location for external extensions originating from crx
  // files: |crx_location|, and originating from update URLs:
  // |download_location|. If either of the origins is not supported by this
  // provider, then it should be initialized as Extensions::INVALID.
  StatefulExternalExtensionProvider(
      Extension::Location crx_location,
      Extension::Location download_location);
  virtual ~StatefulExternalExtensionProvider();

  // ExternalExtensionProvider implementation:
  virtual void VisitRegisteredExtension(
      Visitor* visitor, const std::set<std::string>& ids_to_ignore) const;

  virtual bool HasExtension(const std::string& id) const;

  virtual bool GetExtensionDetails(const std::string& id,
                                   Extension::Location* location,
                                   scoped_ptr<Version>* version) const;
 protected:
  // Location for external extensions that are provided by this provider from
  // local crx files.
  const Extension::Location crx_location_;
  // Location for external extensions that are provided by this provider from
  // update URLs.
  const Extension::Location download_location_;
  // Dictionary of the external extensions that are provided by this provider.
  scoped_ptr<DictionaryValue> prefs_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_STATEFUL_EXTERNAL_EXTENSION_PROVIDER_H_
