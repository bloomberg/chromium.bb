// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_EXTENSION_PROVIDER_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_EXTENSION_PROVIDER_IMPL_H_
#pragma once

#include "chrome/browser/extensions/external_extension_provider_interface.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/external_extension_loader.h"

class DictionaryValue;
class ExternalExtensionLoader;
class Profile;
class ValueSerializer;
class Version;

// A specialization of the ExternalExtensionProvider that uses an instance
// of ExternalExtensionLoader to provide external extensions. This class
// can be seen as a bridge between the extension system and an
// ExternalExtensionLoader. Instances live their entire life on the UI thread.
class ExternalExtensionProviderImpl
    : public ExternalExtensionProviderInterface {
 public:
  // The constructed provider will provide the extensions loaded from |loader|
  // to |service|, that will deal with the installation. The location
  // attributes of the provided extensions are also specified here:
  // |crx_location|: extensions originating from crx files
  // |download_location|: extensions originating from update URLs
  // If either of the origins is not supported by this provider, then it should
  // be initialized as Extensions::INVALID.
  ExternalExtensionProviderImpl(
      VisitorInterface* service,
      ExternalExtensionLoader* loader,
      Extension::Location crx_location,
      Extension::Location download_location);

  virtual ~ExternalExtensionProviderImpl();

  // Populates a list with providers for all known sources.
  static void CreateExternalProviders(
      VisitorInterface* service,
      Profile* profile,
      ProviderCollection* provider_list);

  // Sets underlying prefs and notifies provider. Only to be called by the
  // owned ExternalExtensionLoader instance.
  void SetPrefs(DictionaryValue* prefs);

  // ExternalExtensionProvider implementation:
  virtual void VisitRegisteredExtension() const;

  virtual bool HasExtension(const std::string& id) const;

  virtual bool GetExtensionDetails(const std::string& id,
                                   Extension::Location* location,
                                   scoped_ptr<Version>* version) const;

  virtual void ServiceShutdown();

  virtual bool IsReady();

  static const char kLocation[];
  static const char kState[];
  static const char kExternalCrx[];
  static const char kExternalVersion[];
  static const char kExternalUpdateUrl[];

 private:
  // Location for external extensions that are provided by this provider from
  // local crx files.
  const Extension::Location crx_location_;

  // Location for external extensions that are provided by this provider from
  // update URLs.
  const Extension::Location download_location_;

 private:
  // Weak pointer to the object that consumes the external extensions.
  // This is zeroed out by: ServiceShutdown()
  VisitorInterface* service_;  // weak

  // Dictionary of the external extensions that are provided by this provider.
  scoped_ptr<DictionaryValue> prefs_;

  // Indicates that the extensions provided by this provider are loaded
  // entirely.
  bool ready_;

  // The loader that loads the list of external extensions and reports them
  // via |SetPrefs|.
  scoped_refptr<ExternalExtensionLoader> loader_;

  DISALLOW_COPY_AND_ASSIGN(ExternalExtensionProviderImpl);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_EXTENSION_PROVIDER_IMPL_H_
