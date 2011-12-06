// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_EXTENSION_PROVIDER_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_EXTENSION_PROVIDER_IMPL_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/external_extension_provider_interface.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/external_extension_loader.h"

class ExternalExtensionLoader;
class Profile;
class Version;

namespace base {
class DictionaryValue;
}

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
      Extension::Location download_location,
      int creation_flags);

  virtual ~ExternalExtensionProviderImpl();

  // Populates a list with providers for all known sources.
  static void CreateExternalProviders(
      VisitorInterface* service,
      Profile* profile,
      ProviderCollection* provider_list);

  // Sets underlying prefs and notifies provider. Only to be called by the
  // owned ExternalExtensionLoader instance.
  void SetPrefs(base::DictionaryValue* prefs);

  // ExternalExtensionProvider implementation:
  virtual void ServiceShutdown() OVERRIDE;
  virtual void VisitRegisteredExtension() OVERRIDE;
  virtual bool HasExtension(const std::string& id) const OVERRIDE;
  virtual bool GetExtensionDetails(const std::string& id,
                                   Extension::Location* location,
                                   scoped_ptr<Version>* version) const OVERRIDE;

  virtual bool IsReady() const OVERRIDE;

  static const char kLocation[];
  static const char kState[];
  static const char kExternalCrx[];
  static const char kExternalVersion[];
  static const char kExternalUpdateUrl[];
  static const char kSupportedLocales[];

  void set_auto_acknowledge(bool auto_acknowledge) {
    auto_acknowledge_ = auto_acknowledge;
  }

 private:
  // Location for external extensions that are provided by this provider from
  // local crx files.
  const Extension::Location crx_location_;

  // Location for external extensions that are provided by this provider from
  // update URLs.
  const Extension::Location download_location_;

  // Weak pointer to the object that consumes the external extensions.
  // This is zeroed out by: ServiceShutdown()
  VisitorInterface* service_;  // weak

  // Dictionary of the external extensions that are provided by this provider.
  scoped_ptr<base::DictionaryValue> prefs_;

  // Indicates that the extensions provided by this provider are loaded
  // entirely.
  bool ready_;

  // The loader that loads the list of external extensions and reports them
  // via |SetPrefs|.
  scoped_refptr<ExternalExtensionLoader> loader_;

  // Creation flags to use for the extension.  These flags will be used
  // when calling Extenion::Create() by the crx installer.
  int creation_flags_;

  // Whether loaded extensions should be automatically acknowledged, so that
  // the user doesn't see an alert about them.
  bool auto_acknowledge_;

  DISALLOW_COPY_AND_ASSIGN(ExternalExtensionProviderImpl);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_EXTENSION_PROVIDER_IMPL_H_
