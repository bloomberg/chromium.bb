// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_INSTALLATION_VALIDATOR_H_
#define CHROME_INSTALLER_UTIL_INSTALLATION_VALIDATOR_H_
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"

class FilePath;
class Version;

namespace installer {

class InstallationState;
class ProductState;

// A class that validates the state of an installation.  Violations are logged
// via LOG(ERROR).
class InstallationValidator {
 public:
  class ProductBits {
   public:
    // Bits that form the components of an installation type.
    enum {
      CHROME_SINGLE           = 0x01,
      CHROME_MULTI            = 0x02,
      CHROME_FRAME_SINGLE     = 0x04,
      CHROME_FRAME_MULTI      = 0x08,
      CHROME_FRAME_READY_MODE = 0x10,
    };
  };  // class ProductBits

  // Identifiers of all valid installation types.
  enum InstallationType {
    NO_PRODUCTS = 0,
    CHROME_SINGLE =
        ProductBits::CHROME_SINGLE,
    CHROME_MULTI =
        ProductBits::CHROME_MULTI,
    CHROME_FRAME_SINGLE =
        ProductBits::CHROME_FRAME_SINGLE,
    CHROME_FRAME_SINGLE_CHROME_SINGLE =
        ProductBits::CHROME_FRAME_SINGLE | ProductBits::CHROME_SINGLE,
    CHROME_FRAME_SINGLE_CHROME_MULTI =
        ProductBits::CHROME_FRAME_SINGLE | ProductBits::CHROME_MULTI,
    CHROME_FRAME_MULTI =
        ProductBits::CHROME_FRAME_MULTI,
    CHROME_FRAME_MULTI_CHROME_MULTI =
        ProductBits::CHROME_FRAME_MULTI | ProductBits::CHROME_MULTI,
    CHROME_FRAME_READY_MODE_CHROME_MULTI =
        ProductBits::CHROME_FRAME_READY_MODE | ProductBits::CHROME_MULTI,
  };

  // Validates |machine_state| at user or system level, returning true if valid.
  // |type| is populated in either case, although it is a best-guess when the
  // method returns false.
  static bool ValidateInstallationTypeForState(
      const InstallationState& machine_state,
      bool system_level,
      InstallationType* type);

  // Validates the machine's current installation at user or system level,
  // returning true and populating |type| if valid.
  static bool ValidateInstallationType(bool system_level,
                                       InstallationType* type);

 protected:
  typedef std::vector<std::pair<std::string, bool> > SwitchExpectations;

  // An interface to product-specific validation rules.
  class ProductRules {
   public:
    virtual ~ProductRules() { }
    virtual BrowserDistribution::Type distribution_type() const = 0;
    virtual void AddProductSwitchExpectations(
        const InstallationState& machine_state,
        bool system_install,
        const ProductState& product_state,
        SwitchExpectations* expectations) const = 0;
  };

  // Validation rules for the Chrome browser.
  class ChromeRules : public ProductRules {
   public:
    virtual BrowserDistribution::Type distribution_type() const OVERRIDE;
    virtual void AddProductSwitchExpectations(
        const InstallationState& machine_state,
        bool system_install,
        const ProductState& product_state,
        SwitchExpectations* expectations) const OVERRIDE;
  };

  // Validation rules for Chrome Frame.
  class ChromeFrameRules : public ProductRules {
   public:
    virtual BrowserDistribution::Type distribution_type() const OVERRIDE;
    virtual void AddProductSwitchExpectations(
        const InstallationState& machine_state,
        bool system_install,
        const ProductState& product_state,
        SwitchExpectations* expectations) const OVERRIDE;
  };

  struct ProductContext {
    const InstallationState& machine_state;
    bool system_install;
    BrowserDistribution* dist;
    const ProductState& state;
    const ProductRules& rules;
  };

  static void ValidateBinaries(const InstallationState& machine_state,
                               bool system_install,
                               const ProductState& binaries_state,
                               bool* is_valid);
  static void ValidateSetupPath(const ProductContext& ctx,
                                const FilePath& setup_exe,
                                const char* purpose,
                                bool* is_valid);
  static void ValidateCommandExpectations(const ProductContext& ctx,
                                          const CommandLine& command,
                                          const SwitchExpectations& expected,
                                          const char* source,
                                          bool* is_valid);
  static void ValidateUninstallCommand(const ProductContext& ctx,
                                       const CommandLine& command,
                                       const char* source,
                                       bool* is_valid);
  static void ValidateRenameCommand(const ProductContext& ctx,
                                    bool* is_valid);
  static void ValidateOldVersionValues(const ProductContext& ctx,
                                       bool* is_valid);
  static void ValidateMultiInstallProduct(const ProductContext& ctx,
                                          bool* is_valid);
  static void ValidateProduct(const InstallationState& machine_state,
                              bool system_install,
                              const ProductState& product_state,
                              const ProductRules& rules,
                              bool* is_valid);

  // A collection of all valid installation types.
  static const InstallationType kInstallationTypes[];

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InstallationValidator);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_INSTALLATION_VALIDATOR_H_
