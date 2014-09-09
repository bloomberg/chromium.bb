// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_URL_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_URL_HANDLER_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace base {
class DictionaryValue;
}

namespace extensions {

// A structure to hold various URLs like devtools_page, homepage_url, etc
// that may be specified in the manifest of an extension.
struct ManifestURL : public Extension::ManifestData {
  GURL url_;

  // Returns the DevTools Page for this extension.
  static const GURL& GetDevToolsPage(const Extension* extension);

  // Returns the Homepage URL for this extension.
  // If homepage_url was not specified in the manifest,
  // this returns the Google Gallery URL. For third-party extensions,
  // this returns a blank GURL.
  static const GURL GetHomepageURL(const Extension* extension);

  // Returns true if the extension specified a valid home page url in the
  // manifest.
  static bool SpecifiedHomepageURL(const Extension* extension);

  // Returns the Update URL for this extension.
  static const GURL& GetUpdateURL(const Extension* extension);

  // Returns true if this extension's update URL is the extension gallery.
  static bool UpdatesFromGallery(const Extension* extension);
  static bool UpdatesFromGallery(const base::DictionaryValue* manifest);

  // Returns the About Page for this extension.
  static const GURL& GetAboutPage(const Extension* extension);

  // Returns the webstore page URL for this extension.
  static const GURL GetDetailsURL(const Extension* extension);
};

// A structure to hold the chrome URL overrides that may be specified
// in the manifest of an extension.
struct URLOverrides : public Extension::ManifestData {
  typedef std::map<const std::string, GURL> URLOverrideMap;

  // Define out of line constructor/destructor to please Clang.
  URLOverrides();
  virtual ~URLOverrides();

  static const URLOverrideMap&
      GetChromeURLOverrides(const Extension* extension);

  // A map of chrome:// hostnames (newtab, downloads, etc.) to Extension URLs
  // which override the handling of those URLs. (see ExtensionOverrideUI).
  URLOverrideMap chrome_url_overrides_;
};

// Parses the "devtools_page" manifest key.
class DevToolsPageHandler : public ManifestHandler {
 public:
  DevToolsPageHandler();
  virtual ~DevToolsPageHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DevToolsPageHandler);
};

// Parses the "homepage_url" manifest key.
class HomepageURLHandler : public ManifestHandler {
 public:
  HomepageURLHandler();
  virtual ~HomepageURLHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(HomepageURLHandler);
};

// Parses the "update_url" manifest key.
class UpdateURLHandler : public ManifestHandler {
 public:
  UpdateURLHandler();
  virtual ~UpdateURLHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(UpdateURLHandler);
};

// Parses the "about_page" manifest key.
// TODO(sashab): Make this and any other similar handlers extend from the same
// abstract class, URLManifestHandler, which has pure virtual methods for
// detecting the required URL type (relative or absolute) and abstracts the
// URL parsing logic away.
class AboutPageHandler : public ManifestHandler {
 public:
  AboutPageHandler();
  virtual ~AboutPageHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;
  virtual bool Validate(const Extension* extension,
                        std::string* error,
                        std::vector<InstallWarning>* warnings) const OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AboutPageHandler);
};

// Parses the "chrome_url_overrides" manifest key.
class URLOverridesHandler : public ManifestHandler {
 public:
  URLOverridesHandler();
  virtual ~URLOverridesHandler();

  virtual bool Parse(Extension* extension, base::string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(URLOverridesHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_URL_HANDLER_H_
