// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
#define CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/values.h"

class DictionaryValue;
class ListValue;

namespace chromeos {

// Base class for OEM customization document classes.
class CustomizationDocument {
 public:
  CustomizationDocument() {}
  virtual ~CustomizationDocument() {}

  virtual bool LoadManifestFromFile(const FilePath& manifest_path);
  virtual bool LoadManifestFromString(const std::string& manifest);

 protected:
  std::string GetLocaleSpecificString(const std::string& locale,
                                      const std::string& dictionary_name,
                                      const std::string& entry_name) const;

  scoped_ptr<DictionaryValue> root_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomizationDocument);
};

// OEM startup customization document class.
class StartupCustomizationDocument : public CustomizationDocument {
 public:
  StartupCustomizationDocument() {}

  virtual bool LoadManifestFromString(const std::string& manifest);

  const std::string& initial_locale() const { return initial_locale_; }
  const std::string& initial_timezone() const { return initial_timezone_; }
  const std::string& keyboard_layout() const { return keyboard_layout_; }
  const std::string& registration_url() const { return registration_url_; }

  std::string GetHelpPage(const std::string& locale) const;
  std::string GetEULAPage(const std::string& locale) const;

 private:
  typedef std::map<std::string, std::string> VPDMap;

  // Returns HWID for the machine. Declared as virtual to override in tests.
  virtual std::string GetHWID() const;

  // Returns VPD as string. Declared as virtual to override in tests.
  virtual std::string GetVPD() const;

  // Parse VPD file as string and initialize |vpd_map|.
  bool ParseVPD(const std::string& vpd_string, VPDMap* vpd_map);

  // If |attr| exists in |vpd_map|, assign it value to |value|;
  void InitFromVPD(const VPDMap& vpd_map, const char* attr, std::string* value);

  std::string initial_locale_;
  std::string initial_timezone_;
  std::string keyboard_layout_;
  std::string registration_url_;

  DISALLOW_COPY_AND_ASSIGN(StartupCustomizationDocument);
};

// OEM services customization document class.
class ServicesCustomizationDocument : public CustomizationDocument {
 public:
  ServicesCustomizationDocument() {}

  std::string GetInitialStartPage(const std::string& locale) const;
  std::string GetSupportPage(const std::string& locale) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServicesCustomizationDocument);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CUSTOMIZATION_DOCUMENT_H_
