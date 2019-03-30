// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/authpolicy/kerberos_files_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/network_service_util.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace chromeos {

namespace {

// Prefix for KRB5CCNAME environment variable. Defines credential cache type.
constexpr char kKrb5CCFilePrefix[] = "FILE:";
// Directory in the user home to store Kerberos files.
constexpr char kKrb5Directory[] = "kerberos";
// Environment variable pointing to credential cache file.
constexpr char kKrb5CCEnvName[] = "KRB5CCNAME";
// Credential cache file name.
constexpr char kKrb5CCFile[] = "krb5cc";
// Environment variable pointing to Kerberos config file.
constexpr char kKrb5ConfEnvName[] = "KRB5_CONFIG";
// Kerberos config file name.
constexpr char kKrb5ConfFile[] = "krb5.conf";

// Writes |blob| into file <UserPath>/kerberos/|file_name|. First writes into
// temporary file and then replaces existing one.
void WriteFile(const std::string& file_name, const std::string& blob) {
  base::FilePath dir;
  base::PathService::Get(base::DIR_HOME, &dir);
  dir = dir.Append(kKrb5Directory);
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(dir, &error)) {
    LOG(ERROR) << "Failed to create '" << dir.value()
               << "' directory: " << base::File::ErrorToString(error);
    return;
  }
  base::FilePath dest_file = dir.Append(file_name);
  if (!base::ImportantFileWriter::WriteFileAtomically(dest_file, blob)) {
    LOG(ERROR) << "Failed to write file " << dest_file.value();
  }
}

// Put canonicalization settings first depending on user policy. Whatever
// setting comes first wins, so even if krb5.conf sets rdns or
// dns_canonicalize_hostname below, it would get overridden.
std::string AdjustConfig(const std::string& config, bool is_dns_cname_enabled) {
  std::string adjusted_config = base::StringPrintf(
      kKrb5CnameSettings, is_dns_cname_enabled ? "true" : "false");
  adjusted_config.append(config);
  return adjusted_config;
}

}  // namespace

const char* kKrb5CnameSettings =
    "[libdefaults]\n"
    "\tdns_canonicalize_hostname = %s\n"
    "\trdns = false\n";

KerberosFilesHandler::KerberosFilesHandler(
    base::RepeatingClosure get_kerberos_files)
    : get_kerberos_files_(std::move(get_kerberos_files)) {
  // Set environment variables for GSSAPI library.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::FilePath path;
  base::PathService::Get(base::DIR_HOME, &path);
  path = path.Append(kKrb5Directory);
  std::string krb5cc_env_value =
      kKrb5CCFilePrefix + path.Append(kKrb5CCFile).value();
  std::string krb5conf_env_value = path.Append(kKrb5ConfFile).value();
  env->SetVar(kKrb5CCEnvName, krb5cc_env_value);
  env->SetVar(kKrb5ConfEnvName, krb5conf_env_value);

  // Send the environment variables to the network service if it's running
  // out of process.
  if (content::IsOutOfProcessNetworkService()) {
    std::vector<network::mojom::EnvironmentVariablePtr> environment;
    environment.push_back(network::mojom::EnvironmentVariable::New(
        kKrb5CCEnvName, krb5cc_env_value));
    environment.push_back(network::mojom::EnvironmentVariable::New(
        kKrb5ConfEnvName, krb5conf_env_value));
    content::GetNetworkService()->SetEnvironment(std::move(environment));
  }

  // Listen to kDisableAuthNegotiateCnameLookup pref. It might change the
  // Kerberos config.
  negotiate_disable_cname_lookup_.Init(
      prefs::kDisableAuthNegotiateCnameLookup, g_browser_process->local_state(),
      base::BindRepeating(
          &KerberosFilesHandler::OnDisabledAuthNegotiateCnameLookupChanged,
          weak_factory_.GetWeakPtr()));
}

KerberosFilesHandler::~KerberosFilesHandler() = default;

void KerberosFilesHandler::SetFiles(base::Optional<std::string> krb5cc,
                                    base::Optional<std::string> krb5conf) {
  if (krb5cc.has_value()) {
    base::PostTaskWithTraits(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&WriteFile, kKrb5CCFile, krb5cc.value()));
  }
  if (krb5conf.has_value()) {
    base::PostTaskWithTraits(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(
            &WriteFile, kKrb5ConfFile,
            AdjustConfig(krb5conf.value(),
                         !negotiate_disable_cname_lookup_.GetValue())));
  }
}

void KerberosFilesHandler::OnDisabledAuthNegotiateCnameLookupChanged() {
  // Refresh kerberos files to adjust config for changed pref.
  get_kerberos_files_.Run();
}

}  // namespace chromeos
