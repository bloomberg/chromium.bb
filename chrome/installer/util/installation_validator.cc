// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the installation validator.

#include "chrome/installer/util/installation_validator.h"

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "base/version.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"

namespace installer {

BrowserDistribution::Type
    InstallationValidator::ChromeRules::distribution_type() const {
  return BrowserDistribution::CHROME_BROWSER;
}

void InstallationValidator::ChromeRules::AddProductSwitchExpectations(
    const InstallationState& machine_state,
    bool system_install,
    const ProductState& product_state,
    SwitchExpectations* expectations) const {
  const bool is_multi_install =
      product_state.uninstall_command().HasSwitch(switches::kMultiInstall);

  // --chrome should be present iff --multi-install.  This wasn't the case in
  // Chrome 10 (between r68996 and r72497), though, so consider it optional.

  // --chrome-frame --ready-mode should be present iff CF in ready mode.
  const ProductState* cf_state =
      machine_state.GetProductState(system_install,
                                    BrowserDistribution::CHROME_FRAME);
  const bool ready_mode =
      cf_state != NULL &&
      cf_state->uninstall_command().HasSwitch(switches::kChromeFrameReadyMode);
  expectations->push_back(std::make_pair(std::string(switches::kChromeFrame),
                                         ready_mode));
  expectations->push_back(
      std::make_pair(std::string(switches::kChromeFrameReadyMode), ready_mode));
}

BrowserDistribution::Type
    InstallationValidator::ChromeFrameRules::distribution_type() const {
  return BrowserDistribution::CHROME_FRAME;
}

void InstallationValidator::ChromeFrameRules::AddProductSwitchExpectations(
    const InstallationState& machine_state,
    bool system_install,
    const ProductState& product_state,
    SwitchExpectations* expectations) const {
  // --chrome-frame must be present.
  expectations->push_back(std::make_pair(std::string(switches::kChromeFrame),
                                         true));
  // --chrome must not be present.
  expectations->push_back(std::make_pair(std::string(switches::kChrome),
                                         false));
}

BrowserDistribution::Type
    InstallationValidator::ChromeBinariesRules::distribution_type() const {
  return BrowserDistribution::CHROME_BINARIES;
}

void InstallationValidator::ChromeBinariesRules::AddProductSwitchExpectations(
    const InstallationState& machine_state,
    bool system_install,
    const ProductState& product_state,
    SwitchExpectations* expectations) const {
  NOTREACHED();
}

// static
const InstallationValidator::InstallationType
    InstallationValidator::kInstallationTypes[] = {
  NO_PRODUCTS,
  CHROME_SINGLE,
  CHROME_MULTI,
  CHROME_FRAME_SINGLE,
  CHROME_FRAME_SINGLE_CHROME_SINGLE,
  CHROME_FRAME_SINGLE_CHROME_MULTI,
  CHROME_FRAME_MULTI,
  CHROME_FRAME_MULTI_CHROME_MULTI,
  CHROME_FRAME_READY_MODE_CHROME_MULTI
};

// Validates the "quick-enable-cf" Google Update product command.
void InstallationValidator::ValidateQuickEnableCfCommand(
    const ProductContext& ctx,
    const AppCommand& command,
    bool* is_valid) {
  DCHECK(is_valid);

  CommandLine the_command(CommandLine::FromString(command.command_line()));

  ValidateSetupPath(ctx, the_command.GetProgram(), "quick enable cf", is_valid);

  SwitchExpectations expected;

  expected.push_back(
      std::make_pair(std::string(switches::kChromeFrameQuickEnable), true));
  expected.push_back(std::make_pair(std::string(switches::kSystemLevel),
                                    ctx.system_install));
  expected.push_back(std::make_pair(std::string(switches::kMultiInstall),
                                    ctx.state.is_multi_install()));

  ValidateCommandExpectations(ctx, the_command, expected, "quick enable cf",
                              is_valid);

  if (!command.sends_pings()) {
    *is_valid = false;
    LOG(ERROR) << "Quick-enable-cf command is not configured to send pings.";
  }

  if (!command.is_web_accessible()) {
    *is_valid = false;
    LOG(ERROR) << "Quick-enable-cf command is not web accessible.";
  }
}

// Validates a product's set of Google Update product commands against a
// collection of expectations.
void InstallationValidator::ValidateAppCommandExpectations(
    const ProductContext& ctx,
    const CommandExpectations& expectations,
    bool* is_valid) {
  DCHECK(is_valid);

  CommandExpectations the_expectations(expectations);

  AppCommands::CommandMapRange cmd_iterators(
      ctx.state.commands().GetIterators());
  CommandExpectations::iterator expectation;
  for (; cmd_iterators.first != cmd_iterators.second; ++cmd_iterators.first) {
    const std::wstring& cmd_id = cmd_iterators.first->first;
    // Do we have an expectation for this command?
    expectation = the_expectations.find(cmd_id);
    if (expectation != the_expectations.end()) {
      (expectation->second)(ctx, cmd_iterators.first->second, is_valid);
      // Remove this command from the set of expectations since we found it.
      the_expectations.erase(expectation);
    } else {
      *is_valid = false;
      LOG(ERROR) << ctx.dist->GetAppShortCutName()
                 << " has an unexpected Google Update product command named \""
                 << cmd_id << "\".";
    }
  }

  // Report on any expected commands that weren't present.
  CommandExpectations::const_iterator scan(the_expectations.begin());
  CommandExpectations::const_iterator end(the_expectations.end());
  for (; scan != end; ++scan) {
    *is_valid = false;
    LOG(ERROR) << ctx.dist->GetAppShortCutName()
               << " is missing the Google Update product command named \""
               << scan->first << "\".";
  }
}

// Validates the multi-install binaries' Google Update commands.
void InstallationValidator::ValidateBinariesCommands(
    const ProductContext& ctx,
    bool* is_valid) {
  DCHECK(is_valid);

  // The quick-enable-cf command must be present if Chrome is installed either
  // alone or with CF in ready-mode.
  const ChannelInfo& channel = ctx.state.channel();
  const ProductState* chrome_state = ctx.machine_state.GetProductState(
      ctx.system_install, BrowserDistribution::CHROME_BROWSER);
  const ProductState* cf_state = ctx.machine_state.GetProductState(
      ctx.system_install, BrowserDistribution::CHROME_FRAME);

  CommandExpectations expectations;

  if (chrome_state != NULL && (cf_state == NULL || channel.IsReadyMode()))
    expectations[kCmdQuickEnableCf] = &ValidateQuickEnableCfCommand;

  ValidateAppCommandExpectations(ctx, expectations, is_valid);
}

// Validates the multi-install binaries at level |system_level|.
void InstallationValidator::ValidateBinaries(
    const InstallationState& machine_state,
    bool system_install,
    const ProductState& binaries_state,
    bool* is_valid) {
  const ChannelInfo& channel = binaries_state.channel();

  // ap must have -multi
  if (!channel.IsMultiInstall()) {
    *is_valid = false;
    LOG(ERROR) << "Chrome Binaries are missing \"-multi\" in channel name: \""
               << channel.value() << "\"";
  }

  // ap must have -chrome iff Chrome is installed
  const ProductState* chrome_state = machine_state.GetProductState(
      system_install, BrowserDistribution::CHROME_BROWSER);
  if (chrome_state != NULL) {
    if (!channel.IsChrome()) {
      *is_valid = false;
      LOG(ERROR) << "Chrome Binaries are missing \"chrome\" in channel name:"
                 << " \"" << channel.value() << "\"";
    }
  } else if (channel.IsChrome()) {
    *is_valid = false;
    LOG(ERROR) << "Chrome Binaries have \"-chrome\" in channel name, yet Chrome"
                  " is not installed: \"" << channel.value() << "\"";
  }

  // ap must have -chromeframe iff Chrome Frame is installed multi
  const ProductState* cf_state = machine_state.GetProductState(
      system_install, BrowserDistribution::CHROME_FRAME);
  if (cf_state != NULL && cf_state->is_multi_install()) {
    if (!channel.IsChromeFrame()) {
      *is_valid = false;
      LOG(ERROR) << "Chrome Binaries are missing \"-chromeframe\" in channel"
                    " name: \"" << channel.value() << "\"";
    }
  } else if (channel.IsChromeFrame()) {
    *is_valid = false;
    LOG(ERROR) << "Chrome Binaries have \"-chromeframe\" in channel name, yet "
                  "Chrome Frame is not installed multi: \"" << channel.value()
               << "\"";
  }

  // ap must have -readymode iff Chrome Frame is installed in ready-mode
  if (cf_state != NULL &&
      cf_state->uninstall_command().HasSwitch(
          installer::switches::kChromeFrameReadyMode)) {
    if (!channel.IsReadyMode()) {
      *is_valid = false;
      LOG(ERROR) << "Chrome Binaries are missing \"-readymode\" in channel"
                    " name: \"" << channel.value() << "\"";
    }
  } else if (channel.IsReadyMode()) {
    *is_valid = false;
    LOG(ERROR) << "Chrome Binaries have \"-readymode\" in channel name, yet "
                  "Chrome Frame is not in ready mode: \"" << channel.value()
               << "\"";
  }

  // Chrome or Chrome Frame must be present
  if (chrome_state == NULL && cf_state == NULL) {
    *is_valid = false;
    LOG(ERROR) << "Chrome Binaries are present with no other products.";
  }

  // Chrome must be multi-install if present.
  if (chrome_state != NULL && !chrome_state->is_multi_install()) {
    *is_valid = false;
    LOG(ERROR)
        << "Chrome Binaries are present yet Chrome is not multi-install.";
  }

  // Chrome Frame must be multi-install if Chrome is not present.
  if (cf_state != NULL && chrome_state == NULL &&
      !cf_state->is_multi_install()) {
    *is_valid = false;
    LOG(ERROR) << "Chrome Binaries are present without Chrome yet Chrome Frame "
                  "is not multi-install.";
  }

  ChromeBinariesRules binaries_rules;
  ProductContext ctx = {
    machine_state,
    system_install,
    BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BINARIES),
    binaries_state,
    binaries_rules
  };
  ValidateBinariesCommands(ctx, is_valid);
}

// Validates the path to |setup_exe| for the product described by |ctx|.
void InstallationValidator::ValidateSetupPath(const ProductContext& ctx,
                                              const FilePath& setup_exe,
                                              const char* purpose,
                                              bool* is_valid) {
  DCHECK(is_valid);

  BrowserDistribution* bins_dist = ctx.dist;
  if (ctx.state.is_multi_install()) {
    bins_dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BINARIES);
  }

  FilePath expected_path = installer::GetChromeInstallPath(ctx.system_install,
                                                           bins_dist);
  expected_path = expected_path
      .AppendASCII(ctx.state.version().GetString())
      .Append(installer::kInstallerDir)
      .Append(installer::kSetupExe);
  if (!FilePath::CompareEqualIgnoreCase(expected_path.value(),
                                        setup_exe.value())) {
    *is_valid = false;
    LOG(ERROR) << ctx.dist->GetAppShortCutName() << " path to " << purpose
               << " is not " << expected_path.value() << ": "
               << setup_exe.value();
  }
}

// Validates that |command| meets the expectations described in |expected|.
void InstallationValidator::ValidateCommandExpectations(
    const ProductContext& ctx,
    const CommandLine& command,
    const SwitchExpectations& expected,
    const char* source,
    bool* is_valid) {
  for (SwitchExpectations::size_type i = 0, size = expected.size(); i < size;
       ++i) {
    const SwitchExpectations::value_type& expectation = expected[i];
    if (command.HasSwitch(expectation.first) != expectation.second) {
      *is_valid = false;
      LOG(ERROR) << ctx.dist->GetAppShortCutName() << " " << source
                 << (expectation.second ? " is missing" : " has") << " \""
                 << expectation.first << "\""
                 << (expectation.second ? "" : " but shouldn't") << ": "
                 << command.command_line_string();
    }
  }
}

// Validates that |command|, originating from |source|, is formed properly for
// the product described by |ctx|
void InstallationValidator::ValidateUninstallCommand(const ProductContext& ctx,
                                                     const CommandLine& command,
                                                     const char* source,
                                                     bool* is_valid) {
  DCHECK(is_valid);

  ValidateSetupPath(ctx, command.GetProgram(), "uninstaller", is_valid);

  const bool is_multi_install = ctx.state.is_multi_install();
  SwitchExpectations expected;

  expected.push_back(std::make_pair(std::string(switches::kUninstall), true));
  expected.push_back(std::make_pair(std::string(switches::kSystemLevel),
                                    ctx.system_install));
  expected.push_back(std::make_pair(std::string(switches::kMultiInstall),
                                    is_multi_install));
  ctx.rules.AddProductSwitchExpectations(ctx.machine_state,
                                         ctx.system_install,
                                         ctx.state, &expected);

  ValidateCommandExpectations(ctx, command, expected, source, is_valid);
}

// Validates the rename command for the product described by |ctx|.
void InstallationValidator::ValidateRenameCommand(const ProductContext& ctx,
                                                  bool* is_valid) {
  DCHECK(is_valid);
  DCHECK(!ctx.state.rename_cmd().empty());

  CommandLine command = CommandLine::FromString(ctx.state.rename_cmd());

  ValidateSetupPath(ctx, command.GetProgram(), "in-use renamer", is_valid);

  SwitchExpectations expected;

  expected.push_back(std::make_pair(std::string(switches::kRenameChromeExe),
                                    true));
  expected.push_back(std::make_pair(std::string(switches::kSystemLevel),
                                    ctx.system_install));
  expected.push_back(std::make_pair(std::string(switches::kMultiInstall),
                                    ctx.state.is_multi_install()));
  ctx.rules.AddProductSwitchExpectations(ctx.machine_state,
                                         ctx.system_install,
                                         ctx.state, &expected);

  ValidateCommandExpectations(ctx, command, expected, "in-use renamer",
                              is_valid);
}

// Validates the "opv" and "cmd" values for the product described in |ctx|.
void InstallationValidator::ValidateOldVersionValues(
    const ProductContext& ctx,
    bool* is_valid) {
  DCHECK(is_valid);

  // opv and cmd must both be present or both absent
  if (ctx.state.old_version() == NULL) {
    if (!ctx.state.rename_cmd().empty()) {
      *is_valid = false;
      LOG(ERROR) << ctx.dist->GetAppShortCutName()
                 << " has a rename command but no opv: "
                 << ctx.state.rename_cmd();
    }
  } else {
    if (ctx.state.rename_cmd().empty()) {
      *is_valid = false;
      LOG(ERROR) << ctx.dist->GetAppShortCutName()
                 << " has an opv but no rename command: "
                 << ctx.state.old_version()->GetString();
    } else {
      ValidateRenameCommand(ctx, is_valid);
    }
  }
}

// Validates the multi-install state of the product described in |ctx|.
void InstallationValidator::ValidateMultiInstallProduct(
    const ProductContext& ctx,
    bool* is_valid) {
  DCHECK(is_valid);

  const ProductState* binaries =
      ctx.machine_state.GetProductState(ctx.system_install,
                                        BrowserDistribution::CHROME_BINARIES);
  DCHECK(binaries);

  // Version must match that of binaries.
  if (ctx.state.version().CompareTo(binaries->version()) != 0) {
    *is_valid = false;
    LOG(ERROR) << "Version of " << ctx.dist->GetAppShortCutName()
               << " (" << ctx.state.version().GetString() << ") does not "
                  "match that of Chrome Binaries ("
               << binaries->version().GetString() << ").";
  }

  // Channel value must match that of binaries.
  if (!ctx.state.channel().Equals(binaries->channel())) {
    *is_valid = false;
    LOG(ERROR) << "Channel name of " << ctx.dist->GetAppShortCutName()
               << " (" << ctx.state.channel().value()
               << ") does not match that of Chrome Binaries ("
               << binaries->channel().value() << ").";
  }
}

// Validates the Google Update commands for the product described in |ctx|.
void InstallationValidator::ValidateAppCommands(
    const ProductContext& ctx,
    bool* is_valid) {
  DCHECK(is_valid);

  // Products are not expected to have any commands.
  ValidateAppCommandExpectations(ctx, CommandExpectations(), is_valid);
}

// Validates the product described in |product_state| according to |rules|.
void InstallationValidator::ValidateProduct(
    const InstallationState& machine_state,
    bool system_install,
    const ProductState& product_state,
    const ProductRules& rules,
    bool* is_valid) {
  DCHECK(is_valid);
  ProductContext ctx = {
    machine_state,
    system_install,
    BrowserDistribution::GetSpecificDistribution(rules.distribution_type()),
    product_state,
    rules
  };

  ValidateUninstallCommand(ctx, product_state.uninstall_command(),
                           "Google Update uninstall command", is_valid);

  ValidateOldVersionValues(ctx, is_valid);

  if (product_state.is_multi_install())
    ValidateMultiInstallProduct(ctx, is_valid);

  ValidateAppCommands(ctx, is_valid);
}

// static
bool InstallationValidator::ValidateInstallationTypeForState(
    const InstallationState& machine_state,
    bool system_level,
    InstallationType* type) {
  DCHECK(type);
  bool rock_on = true;
  *type = NO_PRODUCTS;

  // Does the system have any multi-installed products?
  const ProductState* multi_state =
      machine_state.GetProductState(system_level,
                                    BrowserDistribution::CHROME_BINARIES);
  if (multi_state != NULL)
    ValidateBinaries(machine_state, system_level, *multi_state, &rock_on);

  // Is Chrome installed?
  const ProductState* product_state =
      machine_state.GetProductState(system_level,
                                    BrowserDistribution::CHROME_BROWSER);
  if (product_state != NULL) {
    ChromeRules chrome_rules;
    ValidateProduct(machine_state, system_level, *product_state,
                    chrome_rules, &rock_on);
    *type = static_cast<InstallationType>(
        *type | (product_state->is_multi_install() ?
                 ProductBits::CHROME_MULTI :
                 ProductBits::CHROME_SINGLE));
  }

  // Is Chrome Frame installed?
  product_state =
      machine_state.GetProductState(system_level,
                                    BrowserDistribution::CHROME_FRAME);
  if (product_state != NULL) {
    ChromeFrameRules chrome_frame_rules;
    ValidateProduct(machine_state, system_level, *product_state,
                    chrome_frame_rules, &rock_on);
    int cf_bit = !product_state->is_multi_install() ?
        ProductBits::CHROME_FRAME_SINGLE :
        (product_state->uninstall_command().HasSwitch(
             switches::kChromeFrameReadyMode) ?
                 ProductBits::CHROME_FRAME_READY_MODE :
                 ProductBits::CHROME_FRAME_MULTI);
    *type = static_cast<InstallationType>(*type | cf_bit);
  }

  DCHECK_NE(std::find(&kInstallationTypes[0],
                      &kInstallationTypes[arraysize(kInstallationTypes)],
                      *type),
            &kInstallationTypes[arraysize(kInstallationTypes)])
      << "Invalid combination of products found on system (" << *type << ")";

  return rock_on;
}

// static
bool InstallationValidator::ValidateInstallationType(bool system_level,
                                                     InstallationType* type) {
  DCHECK(type);
  InstallationState machine_state;

  machine_state.Initialize();

  return ValidateInstallationTypeForState(machine_state, system_level, type);
}

}  // namespace installer
