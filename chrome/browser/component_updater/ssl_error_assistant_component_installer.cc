// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/ssl_error_assistant_component_installer.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ssl/ssl_error_handler.h"
#include "content/public/browser/browser_thread.h"

using component_updater::ComponentUpdateService;

namespace {

const base::FilePath::CharType kConfigBinaryPbFileName[] =
    FILE_PATH_LITERAL("ssl_error_assistant.pb");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: giekcmmlnklenlaomppkphknjmnnpneh
const uint8_t kPublicKeySHA256[32] = {
    0x68, 0x4a, 0x2c, 0xcb, 0xda, 0xb4, 0xdb, 0x0e, 0xcf, 0xfa, 0xf7,
    0xad, 0x9c, 0xdd, 0xfd, 0x47, 0x97, 0xe4, 0x73, 0x24, 0x67, 0x93,
    0x9c, 0xb1, 0x14, 0xcd, 0x3f, 0x54, 0x66, 0x25, 0x99, 0x3f};

void LoadProtoFromDisk(const base::FilePath& pb_path) {
  if (pb_path.empty())
    return;

  std::string binary_pb;
  if (!base::ReadFileToString(pb_path, &binary_pb)) {
    // The file won't exist on new installations, so this is not always an
    // error.
    DVLOG(1) << "Failed reading from " << pb_path.value();
    return;
  }
  auto proto = base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  if (!proto->ParseFromString(binary_pb)) {
    DVLOG(1) << "Failed parsing proto " << pb_path.value();
    return;
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SSLErrorHandler::SetErrorAssistantProto,
                 base::Passed(std::move(proto))));
}

}  // namespace

namespace component_updater {

bool SSLErrorAssistantComponentInstallerTraits::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool SSLErrorAssistantComponentInstallerTraits::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
SSLErrorAssistantComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

base::FilePath SSLErrorAssistantComponentInstallerTraits::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kConfigBinaryPbFileName);
}

void SSLErrorAssistantComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DVLOG(1) << "Component ready, version " << version.GetString() << " in "
           << install_dir.value();

  if (!content::BrowserThread::PostBlockingPoolTask(
          FROM_HERE,
          base::Bind(&LoadProtoFromDisk, GetInstalledPath(install_dir)))) {
    NOTREACHED();
  }
}

// Called during startup and installation before ComponentReady().
bool SSLErrorAssistantComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the proto here, since we'll do the checking
  // in PopulateFromDynamicUpdate().
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath
SSLErrorAssistantComponentInstallerTraits::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("SSLErrorAssistant"));
}

void SSLErrorAssistantComponentInstallerTraits::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kPublicKeySHA256,
               kPublicKeySHA256 + arraysize(kPublicKeySHA256));
}

std::string SSLErrorAssistantComponentInstallerTraits::GetName() const {
  return "SSL Error Assistant";
}

update_client::InstallerAttributes
SSLErrorAssistantComponentInstallerTraits::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string>
SSLErrorAssistantComponentInstallerTraits::GetMimeTypes() const {
  return std::vector<std::string>();
}

void RegisterSSLErrorAssistantComponent(ComponentUpdateService* cus,
                                        const base::FilePath& user_data_dir) {
  DVLOG(1) << "Registering SSL Error Assistant component.";

  std::unique_ptr<ComponentInstallerTraits> traits(
      new SSLErrorAssistantComponentInstallerTraits());
  // |cus| takes ownership of |installer|.
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
}

}  // namespace component_updater
