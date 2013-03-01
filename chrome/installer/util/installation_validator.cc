// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the installation validator.

#include "chrome/installer/util/installation_validator.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"

namespace installer {

BrowserDistribution::Type
    InstallationValidator::ChromeRules::distribution_type() const {
  return BrowserDistribution::CHROME_BROWSER;
}

void InstallationValidator::ChromeRules::AddUninstallSwitchExpectations(
    const ProductContext& ctx,
    SwitchExpectations* expectations) const {
  const bool is_multi_install =
      ctx.state.uninstall_command().HasSwitch(switches::kMultiInstall);

  // --chrome should be present for uninstall iff --multi-install.  This wasn't
  // the case in Chrome 10 (between r68996 and r72497), though, so consider it
  // optional.

  // --chrome-frame --ready-mode should be present for uninstall iff CF in ready
  // mode.
  const ProductState* cf_state =
      ctx.machine_state.GetProductState(ctx.system_install,
                                        BrowserDistribution::CHROME_FRAME);
  const bool ready_mode =
      cf_state != NULL &&
      cf_state->uninstall_command().HasSwitch(switches::kChromeFrameReadyMode);
  expectations->push_back(std::make_pair(std::string(switches::kChromeFrame),
                                         ready_mode));
  expectations->push_back(
      std::make_pair(std::string(switches::kChromeFrameReadyMode), ready_mode));
}

void InstallationValidator::ChromeRules::AddRenameSwitchExpectations(
    const ProductContext& ctx,
    SwitchExpectations* expectations) const {
  const bool is_multi_install =
      ctx.state.uninstall_command().HasSwitch(switches::kMultiInstall);

  // --chrome should not be present for rename.  It was for a time, so we'll be
  // lenient so that mini_installer tests pass.

  // --chrome-frame --ready-mode should never be present.
  expectations->push_back(
      std::make_pair(std::string(switches::kChromeFrame), false));
  expectations->push_back(
      std::make_pair(std::string(switches::kChromeFrameReadyMode), false));
}

bool InstallationValidator::ChromeRules::UsageStatsAllowed(
    const ProductContext& ctx) const {
  // Products must not have usagestats consent values when multi-install
  // (only the multi-install binaries may).
  return !ctx.state.is_multi_install();
}

BrowserDistribution::Type
    InstallationValidator::ChromeFrameRules::distribution_type() const {
  return BrowserDistribution::CHROME_FRAME;
}

void InstallationValidator::ChromeFrameRules::AddUninstallSwitchExpectations(
    const ProductContext& ctx,
    SwitchExpectations* expectations) const {
  // --chrome-frame must be present.
  expectations->push_back(std::make_pair(std::string(switches::kChromeFrame),
                                         true));
  // --chrome must not be present.
  expectations->push_back(std::make_pair(std::string(switches::kChrome),
                                         false));
}

void InstallationValidator::ChromeFrameRules::AddRenameSwitchExpectations(
    const ProductContext& ctx,
    SwitchExpectations* expectations) const {
  // --chrome-frame must be present for SxS rename.
  expectations->push_back(std::make_pair(std::string(switches::kChromeFrame),
                                         !ctx.state.is_multi_install()));
  // --chrome must not be present.
  expectations->push_back(std::make_pair(std::string(switches::kChrome),
                                         false));
}

bool InstallationValidator::ChromeFrameRules::UsageStatsAllowed(
    const ProductContext& ctx) const {
  // Products must not have usagestats consent values when multi-install
  // (only the multi-install binaries may).
  return !ctx.state.is_multi_install();
}

BrowserDistribution::Type
    InstallationValidator::ChromeAppHostRules::distribution_type() const {
  return BrowserDistribution::CHROME_APP_HOST;
}

void InstallationValidator::ChromeAppHostRules::AddUninstallSwitchExpectations(
    const ProductContext& ctx,
    SwitchExpectations* expectations) const {
  DCHECK(!ctx.system_install);

  // --app-launcher must be present.
  expectations->push_back(
      std::make_pair(std::string(switches::kChromeAppLauncher), true));

  // --chrome must not be present.
  expectations->push_back(std::make_pair(std::string(switches::kChrome),
                                         false));
  // --chrome-frame must not be present.
  expectations->push_back(std::make_pair(std::string(switches::kChromeFrame),
                                         false));
}

void InstallationValidator::ChromeAppHostRules::AddRenameSwitchExpectations(
    const ProductContext& ctx,
    SwitchExpectations* expectations) const {
  // TODO(erikwright): I guess there will be none?
}

bool InstallationValidator::ChromeAppHostRules::UsageStatsAllowed(
    const ProductContext& ctx) const {
  // App Host doesn't manage usage stats. The Chrome Binaries will.
  return false;
}

BrowserDistribution::Type
    InstallationValidator::ChromeBinariesRules::distribution_type() const {
  return BrowserDistribution::CHROME_BINARIES;
}

void InstallationValidator::ChromeBinariesRules::AddUninstallSwitchExpectations(
    const ProductContext& ctx,
    SwitchExpectations* expectations) const {
  NOTREACHED();
}

void InstallationValidator::ChromeBinariesRules::AddRenameSwitchExpectations(
    const ProductContext& ctx,
    SwitchExpectations* expectations) const {
  NOTREACHED();
}

bool InstallationValidator::ChromeBinariesRules::UsageStatsAllowed(
    const ProductContext& ctx) const {
  // UsageStats consent values are always allowed on the binaries.
  return true;
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
  CHROME_FRAME_READY_MODE_CHROME_MULTI,
  CHROME_APP_HOST,
  CHROME_APP_HOST_CHROME_FRAME_SINGLE,
  CHROME_APP_HOST_CHROME_FRAME_SINGLE_CHROME_MULTI,
  CHROME_APP_HOST_CHROME_FRAME_MULTI,
  CHROME_APP_HOST_CHROME_FRAME_MULTI_CHROME_MULTI,
  CHROME_APP_HOST_CHROME_MULTI,
  CHROME_APP_HOST_CHROME_MULTI_CHROME_FRAME_READY_MODE,
};

void InstallationValidator::ValidateAppCommandFlags(
    const ProductContext& ctx,
    const AppCommand& app_cmd,
    const std::set<string16>& flags_exp,
    const string16& name,
    bool* is_valid) {
  const struct {
    const string16 exp_key;
    bool val;
    const char* msg;
  } check_list[] = {
    {google_update::kRegSendsPingsField,
         app_cmd.sends_pings(),
         "be configured to send pings"},
    {google_update::kRegWebAccessibleField,
         app_cmd.is_web_accessible(),
         "be web accessible"},
    {google_update::kRegAutoRunOnOSUpgradeField,
         app_cmd.is_auto_run_on_os_upgrade(),
         "be marked to run on OS upgrade"},
    {google_update::kRegRunAsUserField,
         app_cmd.is_run_as_user(),
         "be marked to run as user"},
  };
  for (int i = 0; i < arraysize(check_list); ++i) {
    bool expected = flags_exp.find(check_list[i].exp_key) != flags_exp.end();
    if (check_list[i].val != expected) {
      *is_valid = false;
      LOG(ERROR) << ctx.dist->GetAppShortCutName() << ": "
                 << name << " command should " << (expected ? "" : "not ")
                 << check_list[i].msg << ".";
    }
  }
}

// Validates both "install-application" and "install-extension" depending on
// what is passed in.
void InstallationValidator::ValidateInstallCommand(
    const ProductContext& ctx,
    const AppCommand& app_cmd,
    const wchar_t* expected_command,
    const wchar_t* expected_app_name,
    const char* expected_switch,
    bool* is_valid) {
  DCHECK(is_valid);

  CommandLine cmd_line(CommandLine::FromString(app_cmd.command_line()));
  string16 name(expected_command);

  base::FilePath expected_path(
      installer::GetChromeInstallPath(ctx.system_install, ctx.dist)
      .Append(expected_app_name));

  if (!base::FilePath::CompareEqualIgnoreCase(expected_path.value(),
                                              cmd_line.GetProgram().value())) {
    *is_valid = false;
    LOG(ERROR) << name << "'s path is not "
               << expected_path.value() << ": "
               << cmd_line.GetProgram().value();
  }

  SwitchExpectations expected;
  expected.push_back(std::make_pair(std::string(expected_switch), true));

  ValidateCommandExpectations(ctx, cmd_line, expected, name, is_valid);

  std::set<string16> flags_exp;
  flags_exp.insert(google_update::kRegSendsPingsField);
  flags_exp.insert(google_update::kRegWebAccessibleField);
  flags_exp.insert(google_update::kRegRunAsUserField);
  ValidateAppCommandFlags(ctx, app_cmd, flags_exp, name, is_valid);
}

// Validates the "install-application" Google Update product command.
void InstallationValidator::ValidateInstallAppCommand(
    const ProductContext& ctx,
    const AppCommand& app_cmd,
    bool* is_valid) {
  ValidateInstallCommand(ctx, app_cmd, kCmdInstallApp,
                         installer::kChromeAppHostExe,
                         ::switches::kInstallFromWebstore, is_valid);
}

// Validates the "install-extension" Google Update product command.
void InstallationValidator::ValidateInstallExtensionCommand(
    const ProductContext& ctx,
    const AppCommand& app_cmd,
    bool* is_valid) {
  ValidateInstallCommand(ctx, app_cmd, kCmdInstallExtension,
                         installer::kChromeExe,
                         ::switches::kLimitedInstallFromWebstore, is_valid);
}

// Validates the "on-os-upgrade" Google Update internal command.
void InstallationValidator::ValidateOnOsUpgradeCommand(
    const ProductContext& ctx,
    const AppCommand& app_cmd,
    bool* is_valid) {
  DCHECK(is_valid);

  CommandLine cmd_line(CommandLine::FromString(app_cmd.command_line()));
  string16 name(kCmdOnOsUpgrade);

  ValidateSetupPath(ctx, cmd_line.GetProgram(), name, is_valid);

  SwitchExpectations expected;
  expected.push_back(std::make_pair(std::string(switches::kOnOsUpgrade), true));
  expected.push_back(std::make_pair(std::string(switches::kSystemLevel),
                                    ctx.system_install));
  expected.push_back(std::make_pair(std::string(switches::kMultiInstall),
                                    ctx.state.is_multi_install()));
  // Expecting kChrome if and only if kMultiInstall.
  expected.push_back(std::make_pair(std::string(switches::kChrome),
                                    ctx.state.is_multi_install()));

  ValidateCommandExpectations(ctx, cmd_line, expected, name, is_valid);

  std::set<string16> flags_exp;
  flags_exp.insert(google_update::kRegAutoRunOnOSUpgradeField);
  ValidateAppCommandFlags(ctx, app_cmd, flags_exp, name, is_valid);
}

// Validates the "query-eula-acceptance" Google Update product command.
void InstallationValidator::ValidateQueryEULAAcceptanceCommand(
    const ProductContext& ctx,
    const AppCommand& app_cmd,
    bool* is_valid) {
  DCHECK(is_valid);

  CommandLine cmd_line(CommandLine::FromString(app_cmd.command_line()));
  string16 name(kCmdQueryEULAAcceptance);

  ValidateSetupPath(ctx, cmd_line.GetProgram(), name, is_valid);

  SwitchExpectations expected;
  expected.push_back(std::make_pair(std::string(switches::kQueryEULAAcceptance),
                                    true));
  expected.push_back(std::make_pair(std::string(switches::kSystemLevel),
                                    ctx.system_install));

  ValidateCommandExpectations(ctx, cmd_line, expected, name, is_valid);

  std::set<string16> flags_exp;
  flags_exp.insert(google_update::kRegWebAccessibleField);
  flags_exp.insert(google_update::kRegRunAsUserField);
  ValidateAppCommandFlags(ctx, app_cmd, flags_exp, name, is_valid);
}

// Validates the "quick-enable-cf" Google Update product command.
void InstallationValidator::ValidateQuickEnableCfCommand(
    const ProductContext& ctx,
    const AppCommand& app_cmd,
    bool* is_valid) {
  DCHECK(is_valid);

  CommandLine cmd_line(CommandLine::FromString(app_cmd.command_line()));
  string16 name(kCmdQuickEnableCf);

  ValidateSetupPath(ctx, cmd_line.GetProgram(), name, is_valid);

  SwitchExpectations expected;

  expected.push_back(
      std::make_pair(std::string(switches::kChromeFrameQuickEnable), true));
  expected.push_back(std::make_pair(std::string(switches::kSystemLevel),
                                    ctx.system_install));
  expected.push_back(std::make_pair(std::string(switches::kMultiInstall),
                                    ctx.state.is_multi_install()));

  ValidateCommandExpectations(ctx, cmd_line, expected, name, is_valid);

  std::set<string16> flags_exp;
  flags_exp.insert(google_update::kRegSendsPingsField);
  flags_exp.insert(google_update::kRegWebAccessibleField);
  ValidateAppCommandFlags(ctx, app_cmd, flags_exp, name, is_valid);
}

// Validates the "quick-enable-application-host" Google Update product command.
void InstallationValidator::ValidateQuickEnableApplicationHostCommand(
    const ProductContext& ctx,
    const AppCommand& app_cmd,
    bool* is_valid) {
  DCHECK(is_valid);

  CommandLine cmd_line(CommandLine::FromString(app_cmd.command_line()));
  string16 name(kCmdQuickEnableApplicationHost);

  ValidateSetupPath(ctx, cmd_line.GetProgram(), name, is_valid);

  SwitchExpectations expected;

  expected.push_back(std::make_pair(
      std::string(switches::kChromeAppLauncher), true));
  expected.push_back(std::make_pair(
      std::string(switches::kSystemLevel), false));
  expected.push_back(std::make_pair(
      std::string(switches::kMultiInstall), true));
  expected.push_back(std::make_pair(
      std::string(switches::kEnsureGoogleUpdatePresent), true));

  ValidateCommandExpectations(ctx, cmd_line, expected, name, is_valid);

  std::set<string16> flags_exp;
  flags_exp.insert(google_update::kRegSendsPingsField);
  flags_exp.insert(google_update::kRegWebAccessibleField);
  flags_exp.insert(google_update::kRegRunAsUserField);
  ValidateAppCommandFlags(ctx, app_cmd, flags_exp, name, is_valid);
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
    const string16& cmd_id = cmd_iterators.first->first;
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

  // The quick-enable-cf command must be present if Chrome Binaries are
  // installed and Chrome Frame is not installed (or installed in ready mode).
  const ChannelInfo& channel = ctx.state.channel();
  const ProductState* binaries_state = ctx.machine_state.GetProductState(
      ctx.system_install, BrowserDistribution::CHROME_BINARIES);
  const ProductState* cf_state = ctx.machine_state.GetProductState(
      ctx.system_install, BrowserDistribution::CHROME_FRAME);

  CommandExpectations expectations;

  if (binaries_state != NULL) {
    if (cf_state == NULL || channel.IsReadyMode())
      expectations[kCmdQuickEnableCf] = &ValidateQuickEnableCfCommand;

    expectations[kCmdQuickEnableApplicationHost] =
        &ValidateQuickEnableApplicationHostCommand;

    expectations[kCmdQueryEULAAcceptance] = &ValidateQueryEULAAcceptanceCommand;
  }

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

  // ap must have -applauncher iff Chrome App Launcher is installed multi
  const ProductState* app_host_state = machine_state.GetProductState(
      system_install, BrowserDistribution::CHROME_APP_HOST);
  if (app_host_state != NULL) {
    if (!app_host_state->is_multi_install()) {
      *is_valid = false;
      LOG(ERROR) << "Chrome App Launcher is installed in non-multi mode.";
    }
    if (!channel.IsAppLauncher()) {
      *is_valid = false;
      LOG(ERROR) << "Chrome Binaries are missing \"-applauncher\" in channel"
                    " name: \"" << channel.value() << "\"";
    }
  } else if (channel.IsAppLauncher()) {
    *is_valid = false;
    LOG(ERROR) << "Chrome Binaries have \"-applauncher\" in channel name, yet "
                  "Chrome App Launcher is not installed: \"" << channel.value()
               << "\"";
  }

  // Chrome, Chrome Frame, or App Host must be present
  if (chrome_state == NULL && cf_state == NULL && app_host_state == NULL) {
    *is_valid = false;
    LOG(ERROR) << "Chrome Binaries are present with no other products.";
  }

  // Chrome must be multi-install if present.
  if (chrome_state != NULL && !chrome_state->is_multi_install()) {
    *is_valid = false;
    LOG(ERROR)
        << "Chrome Binaries are present yet Chrome is not multi-install.";
  }

  // Chrome Frame must be multi-install if Chrome & App Host are not present.
  if (cf_state != NULL && app_host_state == NULL && chrome_state == NULL &&
      !cf_state->is_multi_install()) {
    *is_valid = false;
    LOG(ERROR) << "Chrome Binaries are present without Chrome nor App Launcher "
               << "yet Chrome Frame is not multi-install.";
  }

  ChromeBinariesRules binaries_rules;
  ProductContext ctx(machine_state, system_install, binaries_state,
                     binaries_rules);

  ValidateBinariesCommands(ctx, is_valid);

  ValidateUsageStats(ctx, is_valid);
}

// Validates the path to |setup_exe| for the product described by |ctx|.
void InstallationValidator::ValidateSetupPath(const ProductContext& ctx,
                                              const base::FilePath& setup_exe,
                                              const string16& purpose,
                                              bool* is_valid) {
  DCHECK(is_valid);

  BrowserDistribution* bins_dist = ctx.dist;
  if (ctx.state.is_multi_install()) {
    bins_dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BINARIES);
  }

  base::FilePath expected_path = installer::GetChromeInstallPath(
      ctx.system_install, bins_dist);
  expected_path = expected_path
      .AppendASCII(ctx.state.version().GetString())
      .Append(installer::kInstallerDir)
      .Append(installer::kSetupExe);
  if (!base::FilePath::CompareEqualIgnoreCase(expected_path.value(),
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
    const string16& source,
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
                 << command.GetCommandLineString();
    }
  }
}

// Validates that |command|, originating from |source|, is formed properly for
// the product described by |ctx|
void InstallationValidator::ValidateUninstallCommand(const ProductContext& ctx,
                                                     const CommandLine& command,
                                                     const string16& source,
                                                     bool* is_valid) {
  DCHECK(is_valid);

  ValidateSetupPath(ctx, command.GetProgram(), ASCIIToUTF16("uninstaller"),
                    is_valid);

  const bool is_multi_install = ctx.state.is_multi_install();
  SwitchExpectations expected;

  expected.push_back(std::make_pair(std::string(switches::kUninstall), true));
  expected.push_back(std::make_pair(std::string(switches::kSystemLevel),
                                    ctx.system_install));
  expected.push_back(std::make_pair(std::string(switches::kMultiInstall),
                                    is_multi_install));
  ctx.rules.AddUninstallSwitchExpectations(ctx, &expected);

  ValidateCommandExpectations(ctx, command, expected, source, is_valid);
}

// Validates the rename command for the product described by |ctx|.
void InstallationValidator::ValidateRenameCommand(const ProductContext& ctx,
                                                  bool* is_valid) {
  DCHECK(is_valid);
  DCHECK(!ctx.state.rename_cmd().empty());

  CommandLine command = CommandLine::FromString(ctx.state.rename_cmd());
  string16 name(ASCIIToUTF16("in-use renamer"));

  ValidateSetupPath(ctx, command.GetProgram(), name, is_valid);

  SwitchExpectations expected;

  expected.push_back(std::make_pair(std::string(switches::kRenameChromeExe),
                                    true));
  expected.push_back(std::make_pair(std::string(switches::kSystemLevel),
                                    ctx.system_install));
  expected.push_back(std::make_pair(std::string(switches::kMultiInstall),
                                    ctx.state.is_multi_install()));
  ctx.rules.AddRenameSwitchExpectations(ctx, &expected);

  ValidateCommandExpectations(ctx, command, expected, name, is_valid);
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
  if (!binaries) {
    if (ctx.dist->GetType() == BrowserDistribution::CHROME_APP_HOST) {
      if (!ctx.machine_state.GetProductState(
              true,  // system-level
              BrowserDistribution::CHROME_BINARIES) &&
          !ctx.machine_state.GetProductState(
              true,  // system-level
              BrowserDistribution::CHROME_BROWSER)) {
        *is_valid = false;
        LOG(ERROR) << ctx.dist->GetAppShortCutName()
                   << " (" << ctx.state.version().GetString() << ") is "
                   << "installed without Chrome Binaries or a system-level "
                   << "Chrome.";
      }
    } else {
      *is_valid = false;
      LOG(ERROR) << ctx.dist->GetAppShortCutName()
                 << " (" << ctx.state.version().GetString() << ") is installed "
                 << "without Chrome Binaries.";
    }
  } else {
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
}

// Validates the Google Update commands for the product described in |ctx|.
void InstallationValidator::ValidateAppCommands(
    const ProductContext& ctx,
    bool* is_valid) {
  DCHECK(is_valid);

  CommandExpectations expectations;

  if (ctx.dist->GetType() == BrowserDistribution::CHROME_APP_HOST) {
    expectations[kCmdInstallApp] = &ValidateInstallAppCommand;
  }
  if (ctx.dist->GetType() == BrowserDistribution::CHROME_BROWSER) {
    expectations[kCmdInstallExtension] = &ValidateInstallExtensionCommand;
    expectations[kCmdOnOsUpgrade] = &ValidateOnOsUpgradeCommand;
  }

  ValidateAppCommandExpectations(ctx, expectations, is_valid);
}

// Validates usagestats for the product or binaries in |ctx|.
void InstallationValidator::ValidateUsageStats(const ProductContext& ctx,
                                               bool* is_valid) {
  DWORD usagestats = 0;
  if (ctx.state.GetUsageStats(&usagestats)) {
    if (!ctx.rules.UsageStatsAllowed(ctx)) {
      *is_valid = false;
      LOG(ERROR) << ctx.dist->GetAppShortCutName()
                 << " has a usagestats value (" << usagestats
                 << "), yet should not.";
    } else if (usagestats != 0 && usagestats != 1) {
      *is_valid = false;
      LOG(ERROR) << ctx.dist->GetAppShortCutName()
                 << " has an unsupported usagestats value (" << usagestats
                 << ").";
    }
  }
}

// Validates the product described in |product_state| according to |rules|.
void InstallationValidator::ValidateProduct(
    const InstallationState& machine_state,
    bool system_install,
    const ProductState& product_state,
    const ProductRules& rules,
    bool* is_valid) {
  DCHECK(is_valid);

  ProductContext ctx(machine_state, system_install, product_state, rules);

  ValidateUninstallCommand(ctx, ctx.state.uninstall_command(),
                           ASCIIToUTF16("Google Update uninstall command"),
                           is_valid);

  ValidateOldVersionValues(ctx, is_valid);

  if (ctx.state.is_multi_install())
    ValidateMultiInstallProduct(ctx, is_valid);

  ValidateAppCommands(ctx, is_valid);

  ValidateUsageStats(ctx, is_valid);
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

  // Is Chrome App Host installed?
  product_state =
      machine_state.GetProductState(system_level,
                                    BrowserDistribution::CHROME_APP_HOST);
  if (product_state != NULL) {
    ChromeAppHostRules chrome_app_host_rules;
    ValidateProduct(machine_state, system_level, *product_state,
                    chrome_app_host_rules, &rock_on);
    *type = static_cast<InstallationType>(*type | ProductBits::CHROME_APP_HOST);
    if (system_level) {
      LOG(ERROR) <<
          "Chrome App Launcher must not be installed at system level.";
      rock_on = false;
    }
    if (!product_state->is_multi_install()) {
      LOG(ERROR) << "Chrome App Launcher must always be multi-install.";
      rock_on = false;
    }
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
