// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sth_set_component_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/safe_json/safe_json_parser.h"
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

STHSetComponentInstallerTraits::STHSetComponentInstallerTraits(
    std::unique_ptr<net::ct::STHObserver> sth_observer)
    : sth_observer_(std::move(sth_observer)) {}

STHSetComponentInstallerTraits::~STHSetComponentInstallerTraits() {}

bool STHSetComponentInstallerTraits::CanAutoUpdate() const {
  return true;
}

// Public data is delivered via this component, no need for encryption.
bool STHSetComponentInstallerTraits::RequiresNetworkEncryption() const {
  return false;
}

bool STHSetComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return true;  // Nothing custom here.
}

void STHSetComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  if (!content::BrowserThread::PostBlockingPoolTask(
          FROM_HERE,
          base::Bind(&STHSetComponentInstallerTraits::LoadSTHsFromDisk,
                     base::Unretained(this), GetInstalledPath(install_dir),
                     version))) {
    NOTREACHED();
  }
}

// Called during startup and installation before ComponentReady().
bool STHSetComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath STHSetComponentInstallerTraits::GetBaseDirectory() const {
  base::FilePath result;
  PathService::Get(DIR_CERT_TRANS_TREE_STATES, &result);
  return result;
}

void STHSetComponentInstallerTraits::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPublicKeySHA256), std::end(kPublicKeySHA256));
}

std::string STHSetComponentInstallerTraits::GetName() const {
  return kSTHSetFetcherManifestName;
}

std::string STHSetComponentInstallerTraits::GetAp() const {
  return std::string();
}

void STHSetComponentInstallerTraits::LoadSTHsFromDisk(
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
    safe_json::SafeJsonParser::Parse(
        json_sth,
        base::Bind(&STHSetComponentInstallerTraits::OnJsonParseSuccess,
                   base::Unretained(this), log_id),
        base::Bind(&STHSetComponentInstallerTraits::OnJsonParseError,
                   base::Unretained(this), log_id));
  }
}

void STHSetComponentInstallerTraits::OnJsonParseSuccess(
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
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&net::ct::STHObserver::NewSTHObserved,
                 base::Unretained(sth_observer_.get()), signed_tree_head));
}

void STHSetComponentInstallerTraits::OnJsonParseError(
    const std::string& log_id,
    const std::string& error) {
  DVLOG(1) << "STH loading failed: " << error
           << " for log: " << base::HexEncode(log_id.data(), log_id.length());
}

void RegisterSTHSetComponent(ComponentUpdateService* cus,
                             const base::FilePath& user_data_dir) {
  DVLOG(1) << "Registering STH Set fetcher component.";

  // TODO(eranm): The next step in auditing CT logs (crbug.com/506227) is to
  // pass the distributor to the IOThread so it can be used in a per-profile
  // context for checking inclusion of SCTs.
  std::unique_ptr<net::ct::STHDistributor> distributor(
      new net::ct::STHDistributor());

  std::unique_ptr<ComponentInstallerTraits> traits(
      new STHSetComponentInstallerTraits(std::move(distributor)));
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
}

}  // namespace component_updater
