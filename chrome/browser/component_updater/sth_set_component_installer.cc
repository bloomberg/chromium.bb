// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sth_set_component_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/net/sth_distributor_provider.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "net/cert/ct_log_response_parser.h"
#include "net/cert/signed_tree_head.h"
#include "net/cert/sth_distributor.h"
#include "net/cert/sth_observer.h"

using component_updater::ComponentUpdateService;

namespace {
const base::FilePath::CharType kSTHsDirName[] = FILE_PATH_LITERAL("sths");

base::FilePath GetInstalledPath(const base::FilePath& base) {
  return base.Append(FILE_PATH_LITERAL("_platform_specific"))
      .Append(FILE_PATH_LITERAL("all"))
      .Append(kSTHsDirName);
}

}  // namespace

namespace component_updater {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: ojjgnpkioondelmggbekfhllhdaimnho
const uint8_t kPublicKeySHA256[32] = {
    0xe9, 0x96, 0xdf, 0xa8, 0xee, 0xd3, 0x4b, 0xc6, 0x61, 0x4a, 0x57,
    0xbb, 0x73, 0x08, 0xcd, 0x7e, 0x51, 0x9b, 0xcc, 0x69, 0x08, 0x41,
    0xe1, 0x96, 0x9f, 0x7c, 0xb1, 0x73, 0xef, 0x16, 0x80, 0x0a};

const char kSTHSetFetcherManifestName[] = "Signed Tree Heads";

STHSetComponentInstallerPolicy::STHSetComponentInstallerPolicy(
    net::ct::STHObserver* sth_observer)
    : sth_observer_(sth_observer), weak_ptr_factory_(this) {}

STHSetComponentInstallerPolicy::~STHSetComponentInstallerPolicy() {}

bool STHSetComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

// Public data is delivered via this component, no need for encryption.
bool STHSetComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
STHSetComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void STHSetComponentInstallerPolicy::OnCustomUninstall() {}

void STHSetComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BACKGROUND, base::MayBlock()},
      base::BindOnce(&STHSetComponentInstallerPolicy::LoadSTHsFromDisk,
                     weak_ptr_factory_.GetWeakPtr(),
                     GetInstalledPath(install_dir), version));
}

// Called during startup and installation before ComponentReady().
bool STHSetComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath STHSetComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("CertificateTransparency"));
}

void STHSetComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPublicKeySHA256), std::end(kPublicKeySHA256));
}

std::string STHSetComponentInstallerPolicy::GetName() const {
  return kSTHSetFetcherManifestName;
}

update_client::InstallerAttributes
STHSetComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> STHSetComponentInstallerPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

void STHSetComponentInstallerPolicy::LoadSTHsFromDisk(
    const base::FilePath& sths_path,
    const base::Version& version) {
  if (sths_path.empty())
    return;

  base::FileEnumerator sth_file_enumerator(sths_path, false,
                                           base::FileEnumerator::FILES,
                                           FILE_PATH_LITERAL("*.sth"));
  base::FilePath sth_file_path;

  while (!(sth_file_path = sth_file_enumerator.Next()).empty()) {
    DVLOG(1) << "Reading STH from file: " << sth_file_path.value();

    const std::string log_id_hex =
        sth_file_path.BaseName().RemoveExtension().MaybeAsASCII();
    if (log_id_hex.empty()) {
      DVLOG(1) << "Error extracting log_id from: "
               << sth_file_path.BaseName().LossyDisplayName();
      continue;
    }

    std::vector<uint8_t> decoding_output;
    if (!base::HexStringToBytes(log_id_hex, &decoding_output)) {
      DVLOG(1) << "Failed to decode Log ID: " << log_id_hex;
      continue;
    }

    const std::string log_id(reinterpret_cast<const char*>(&decoding_output[0]),
                             decoding_output.size());

    std::string json_sth;
    if (!base::ReadFileToString(sth_file_path, &json_sth)) {
      DVLOG(1) << "Failed reading from " << sth_file_path.value();
      continue;
    }

    DVLOG(1) << "STH: Successfully read: " << json_sth;

    int error_code = 0;
    std::string error_message;
    std::unique_ptr<base::Value> parsed_json =
        base::JSONReader::ReadAndReturnError(json_sth, base::JSON_PARSE_RFC,
                                             &error_code, &error_message);

    if (error_code == base::JSONReader::JSON_NO_ERROR) {
      OnJsonParseSuccess(log_id, std::move(parsed_json));
    } else {
      OnJsonParseError(log_id, error_message);
    }
  }
}

void STHSetComponentInstallerPolicy::OnJsonParseSuccess(
    const std::string& log_id,
    std::unique_ptr<base::Value> parsed_json) {
  net::ct::SignedTreeHead signed_tree_head;
  DVLOG(1) << "STH parsing success for log: "
           << base::HexEncode(log_id.data(), log_id.length());
  if (!net::ct::FillSignedTreeHead(*(parsed_json.get()), &signed_tree_head)) {
    LOG(ERROR) << "Failed to fill in signed tree head.";
    return;
  }

  // The log id is not a part of the response, fill in manually.
  signed_tree_head.log_id = log_id;
  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&net::ct::STHObserver::NewSTHObserved,
                         base::Unretained(sth_observer_), signed_tree_head));
}

void STHSetComponentInstallerPolicy::OnJsonParseError(
    const std::string& log_id,
    const std::string& error) {
  DVLOG(1) << "STH loading failed: " << error
           << " for log: " << base::HexEncode(log_id.data(), log_id.length());
}

void RegisterSTHSetComponent(ComponentUpdateService* cus,
                             const base::FilePath& user_data_dir) {
  DVLOG(1) << "Registering STH Set fetcher component.";

  net::ct::STHDistributor* distributor =
      chrome_browser_net::GetGlobalSTHDistributor();
  // The global STHDistributor should have been created by this point.
  DCHECK(distributor);

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<STHSetComponentInstallerPolicy>(distributor));
  installer->Register(cus, base::Closure());
}

}  // namespace component_updater
