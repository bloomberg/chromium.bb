// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_AUTHPOLICY_KERBEROS_FILES_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_AUTHPOLICY_KERBEROS_FILES_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/prefs/pref_member.h"

namespace chromeos {

// Kerberos defaults for canonicalization SPN. (see
// https://web.mit.edu/kerberos/krb5-1.12/doc/admin/conf_files/krb5_conf.html)
// Exported for browsertests.
extern const char* kKrb5CnameSettings;

// Helper class to update Kerberos credential cache and config files used by
// Chrome for Kerberos authentication.
class KerberosFilesHandler {
 public:
  explicit KerberosFilesHandler(base::RepeatingClosure get_kerberos_files);
  ~KerberosFilesHandler();

  void SetFiles(base::Optional<std::string> krb5cc,
                base::Optional<std::string> krb5conf);

 private:
  // Called whenever prefs::kDisableAuthNegotiateCnameLookup is changed.
  void OnDisabledAuthNegotiateCnameLookupChanged();

  PrefMember<bool> negotiate_disable_cname_lookup_;

  // Triggers a fetch of Kerberos files. Called when the watched pref changes.
  base::RepeatingClosure get_kerberos_files_;

  base::WeakPtrFactory<KerberosFilesHandler> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(KerberosFilesHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_AUTHPOLICY_KERBEROS_FILES_HANDLER_H_
