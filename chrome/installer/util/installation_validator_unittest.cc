// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installation_validator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using installer::ChannelInfo;
using installer::InstallationValidator;
using installer::InstallationState;
using installer::ProductState;
using testing::_;
using testing::StrictMock;

namespace {

enum Channel {
  STABLE_CHANNEL,
  BETA_CHANNEL,
  DEV_CHANNEL
};

enum PackageType {
  SINGLE_INSTALL,
  MULTI_INSTALL
};

enum Level {
  USER_LEVEL,
  SYSTEM_LEVEL
};

enum Vehicle {
  GOOGLE_UPDATE,
  MSI
};

enum ChannelModifier {
  CM_MULTI        = 0x01,
  CM_CHROME       = 0x02,
  CM_CHROME_FRAME = 0x04,
  CM_READY_MODE   = 0x08,
  CM_FULL         = 0x10
};

const wchar_t* const kChromeChannels[] = {
  L"",
  L"1.1-beta",
  L"2.0-dev"
};

const wchar_t* const kChromeFrameChannels[] = {
  L"",
  L"beta",
  L"dev"
};

class FakeProductState : public ProductState {
 public:
  void Clear();
  void SetChannel(const wchar_t* base, int channel_modifiers);
  void SetVersion(const char* version);
  void SetUninstallCommand(BrowserDistribution::Type dist_type,
                           Level install_level,
                           const char* version,
                           int channel_modifiers,
                           Vehicle vehicle);
  void set_multi_install(bool is_multi_install) {
    multi_install_ = is_multi_install;
  }

 protected:
  struct ChannelMethodForModifier {
    ChannelModifier modifier;
    bool (ChannelInfo::*method)(bool value);
  };

  static const ChannelMethodForModifier kChannelMethods[];
};

class FakeInstallationState : public InstallationState {
 public:
  void SetProductState(BrowserDistribution::Type type,
                       Level install_level,
                       const ProductState& product) {
    GetProducts(install_level)[IndexFromDistType(type)].CopyFrom(product);
  }

 protected:
  ProductState* GetProducts(Level install_level) {
    return install_level == USER_LEVEL ? user_products_ : system_products_;
  }
};

// static
const FakeProductState::ChannelMethodForModifier
    FakeProductState::kChannelMethods[] = {
  { CM_MULTI,        &ChannelInfo::SetMultiInstall },
  { CM_CHROME,       &ChannelInfo::SetChrome },
  { CM_CHROME_FRAME, &ChannelInfo::SetChromeFrame },
  { CM_READY_MODE,   &ChannelInfo::SetReadyMode },
  { CM_FULL,         &ChannelInfo::SetFullSuffix }
};

void FakeProductState::Clear() {
  channel_.set_value(std::wstring());
  version_.reset();
  old_version_.reset();
  rename_cmd_.clear();
  uninstall_command_ = CommandLine(CommandLine::NO_PROGRAM);
  msi_ = false;
  multi_install_ = false;
}

// Sets the channel_ member of this instance according to a base channel value
// and a set of modifiers.
void FakeProductState::SetChannel(const wchar_t* base, int channel_modifiers) {
  channel_.set_value(base);
  for (size_t i = 0; i < arraysize(kChannelMethods); ++i) {
    if ((channel_modifiers & kChannelMethods[i].modifier) != 0)
      (channel_.*kChannelMethods[i].method)(true);
  }
}

void FakeProductState::SetVersion(const char* version) {
  version_.reset(
      version == NULL ? NULL : Version::GetVersionFromString(version));
}

// Sets the uninstall command for this object.
void FakeProductState::SetUninstallCommand(BrowserDistribution::Type dist_type,
                                           Level install_level,
                                           const char* version,
                                           int channel_modifiers,
                                           Vehicle vehicle) {
  DCHECK(version);

  const bool is_multi_install = (channel_modifiers & CM_MULTI) != 0;
  FilePath setup_path = installer::GetChromeInstallPath(
      install_level == SYSTEM_LEVEL,
      BrowserDistribution::GetSpecificDistribution(is_multi_install ?
              BrowserDistribution::CHROME_BINARIES : dist_type));
  setup_path = setup_path
      .AppendASCII(version)
      .Append(installer::kInstallerDir)
      .Append(installer::kSetupExe);
  uninstall_command_ = CommandLine(setup_path);
  uninstall_command_.AppendSwitch(installer::switches::kUninstall);
  if (install_level == SYSTEM_LEVEL)
    uninstall_command_.AppendSwitch(installer::switches::kSystemLevel);
  if (is_multi_install) {
    uninstall_command_.AppendSwitch(installer::switches::kMultiInstall);
    if (dist_type == BrowserDistribution::CHROME_BROWSER) {
      uninstall_command_.AppendSwitch(installer::switches::kChrome);
      if ((channel_modifiers & CM_READY_MODE) != 0) {
        uninstall_command_.AppendSwitch(installer::switches::kChromeFrame);
        uninstall_command_.AppendSwitch(
            installer::switches::kChromeFrameReadyMode);
      }
    }
  } else if (dist_type == BrowserDistribution::CHROME_FRAME) {
    uninstall_command_.AppendSwitch(installer::switches::kChromeFrame);
  }
  if (vehicle == MSI)
    uninstall_command_.AppendSwitch(installer::switches::kMsi);
}

// Populates |chrome_state| with the state of a valid Chrome browser
// installation.  |channel_modifiers|, a field of bits defined by enum
// ChannelModifier, dictate properties of the installation (multi-install,
// ready-mode, etc).
void MakeChromeState(Level install_level,
                     Channel channel,
                     int channel_modifiers,
                     Vehicle vehicle,
                     FakeProductState* chrome_state) {
  chrome_state->Clear();
  chrome_state->SetChannel(kChromeChannels[channel], channel_modifiers);
  chrome_state->SetVersion(chrome::kChromeVersion);
  chrome_state->SetUninstallCommand(BrowserDistribution::CHROME_BROWSER,
                                    install_level, chrome::kChromeVersion,
                                    channel_modifiers, vehicle);
  chrome_state->set_multi_install((channel_modifiers & CM_MULTI) != 0);
}

}  // namespace

// Fixture for testing the InstallationValidator.  Errors logged by the
// validator are sent to an optional mock recipient (see
// set_validation_error_recipient) upon which expectations can be placed.
class InstallationValidatorTest : public testing::Test {
 protected:
  class ValidationErrorRecipient {
   public:
    virtual ~ValidationErrorRecipient() { }
    virtual void ReceiveValidationError(const char* file,
                                        int line,
                                        const char* message) = 0;
  };
  class MockValidationErrorRecipient : public ValidationErrorRecipient {
   public:
    MOCK_METHOD3(ReceiveValidationError, void(const char* file,
                                              int line,
                                              const char* message));
  };

  static void SetUpTestCase();
  static void TearDownTestCase();
  static bool HandleLogMessage(int severity,
                               const char* file,
                               int line,
                               size_t message_start,
                               const std::string& str);
  static void set_validation_error_recipient(
      ValidationErrorRecipient* recipient);

  virtual void TearDown();

  static logging::LogMessageHandlerFunction old_log_message_handler_;
  static ValidationErrorRecipient* validation_error_recipient_;
};

// static
logging::LogMessageHandlerFunction
    InstallationValidatorTest::old_log_message_handler_ = NULL;

// static
InstallationValidatorTest::ValidationErrorRecipient*
    InstallationValidatorTest::validation_error_recipient_ = NULL;

// static
void InstallationValidatorTest::SetUpTestCase() {
  old_log_message_handler_ = logging::GetLogMessageHandler();
  logging::SetLogMessageHandler(&HandleLogMessage);
}

// static
void InstallationValidatorTest::TearDownTestCase() {
  logging::SetLogMessageHandler(old_log_message_handler_);
  old_log_message_handler_ = NULL;
}

// static
bool InstallationValidatorTest::HandleLogMessage(int severity,
                                                 const char* file,
                                                 int line,
                                                 size_t message_start,
                                                 const std::string& str) {
  // All validation failures result in LOG(ERROR)
  if (severity == logging::LOG_ERROR) {
    if (validation_error_recipient_ != NULL) {
      validation_error_recipient_->ReceiveValidationError(
          file, line, str.c_str() + message_start);
    } else {
      // Fail the test if an error wasn't handled.
      ADD_FAILURE_AT(file, line) << (str.c_str() + message_start);
    }
    return true;
  }

  if (old_log_message_handler_ != NULL)
    return (old_log_message_handler_)(severity, file, line, message_start, str);

  return false;
}

// static
void InstallationValidatorTest::set_validation_error_recipient(
    ValidationErrorRecipient* recipient) {
  validation_error_recipient_ = recipient;
}

void InstallationValidatorTest::TearDown() {
  validation_error_recipient_ = NULL;
}

// Test that NO_PRODUCTS is returned.
TEST_F(InstallationValidatorTest, NoProducts) {
  InstallationState empty_state;
  InstallationValidator::InstallationType type =
      static_cast<InstallationValidator::InstallationType>(-1);
  StrictMock<MockValidationErrorRecipient> recipient;
  set_validation_error_recipient(&recipient);

  EXPECT_TRUE(InstallationValidator::ValidateInstallationTypeForState(
                  empty_state, true, &type));
  EXPECT_EQ(InstallationValidator::NO_PRODUCTS, type);
}

// Test valid single Chrome.
TEST_F(InstallationValidatorTest, ChromeVersion) {
  FakeProductState chrome_state;
  FakeInstallationState machine_state;
  InstallationValidator::InstallationType type;
  StrictMock<MockValidationErrorRecipient> recipient;
  set_validation_error_recipient(&recipient);

  MakeChromeState(SYSTEM_LEVEL, STABLE_CHANNEL, 0, GOOGLE_UPDATE,
                  &chrome_state);
  machine_state.SetProductState(BrowserDistribution::CHROME_BROWSER,
                                SYSTEM_LEVEL, chrome_state);
  EXPECT_TRUE(InstallationValidator::ValidateInstallationTypeForState(
                  machine_state, true, &type));
  EXPECT_EQ(InstallationValidator::CHROME_SINGLE, type);
}
