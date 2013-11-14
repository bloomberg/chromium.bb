// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_VERIFIER_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_VERIFIER_H_

#include <queue>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/common/extensions/extension.h"

namespace net {
class URLRequestContextGetter;
}

namespace extensions {

class ExtensionPrefs;
class InstallSigner;
struct InstallSignature;

// This class implements verification that a set of extensions are either from
// the webstore or are whitelisted by enterprise policy.  The webstore
// verification process works by sending a request to a backend server to get a
// signature proving that a set of extensions are verified. This signature is
// written into the extension preferences and is checked for validity when
// being read back again.
//
// This class should be kept notified of runtime changes to the set of
// extensions installed from the webstore.
class InstallVerifier : public ManagementPolicy::Provider {
 public:
  InstallVerifier(ExtensionPrefs* prefs,
                  net::URLRequestContextGetter* context_getter);
  virtual ~InstallVerifier();

  // Initializes this object for use, including reading preferences and
  // validating the stored signature.
  void Init();

  // A callback for indicating success/failure of adding new ids.
  typedef base::Callback<void(bool)> AddResultCallback;

  // Try adding a new |id| (or set of ids) to the list of verified ids. When
  // this process is finished |callback| will be run with success/failure. In
  // case of success, subsequent calls to IsVerified will begin returning true
  // for |id|.
  void Add(const std::string& id, const AddResultCallback& callback);
  void AddMany(const ExtensionIdSet& ids,
               const AddResultCallback& callback);

  // Call this to add a set of ids that will immediately be considered allowed,
  // and kick off an aysnchronous request to Add.
  void AddProvisional(const ExtensionIdSet& ids);

  // Removes an id or set of ids from the verified list.
  void Remove(const std::string& id);
  void RemoveMany(const ExtensionIdSet& ids);

  // ManagementPolicy::Provider interface.
  virtual std::string GetDebugPolicyProviderName() const OVERRIDE;
  virtual bool MustRemainDisabled(const Extension* extension,
                                  Extension::DisableReason* reason,
                                  string16* error) const OVERRIDE;

 private:
  // We keep a list of operations to the current set of extensions - either
  // additions or removals.
  enum OperationType {
    ADD,
    REMOVE
  };

  // This is an operation we want to apply to the current set of verified ids.
  struct PendingOperation {
    OperationType type;

    // This is the set of ids being either added or removed.
    ExtensionIdSet ids;

    AddResultCallback callback;

    explicit PendingOperation();
    ~PendingOperation();
  };

  // Removes any no-longer-installed ids, requesting a new signature if needed.
  void GarbageCollect();

  // Returns whether an extension id is allowed by policy.
  bool AllowedByEnterprisePolicy(const std::string& id) const;

  // Returns whether the given |id| is included in our verified signature.
  bool IsVerified(const std::string& id) const;

  // Begins the process of fetching a new signature, based on applying the
  // operation at the head of the queue to the current set of ids in
  // |signature_| (if any) and then sending a request to sign that.
  void BeginFetch();

  // Saves the current value of |signature_| to the prefs;
  void SaveToPrefs();

  // Called with the result of a signature request, or NULL on failure.
  void SignatureCallback(scoped_ptr<InstallSignature> signature);

  ExtensionPrefs* prefs_;
  net::URLRequestContextGetter* context_getter_;

  // This is the most up-to-date signature, read out of |prefs_| during
  // initialization and updated anytime we get new id's added.
  scoped_ptr<InstallSignature> signature_;

  // The current InstallSigner, if we have a signature request running.
  scoped_ptr<InstallSigner> signer_;

  // A queue of operations to apply to the current set of allowed ids.
  std::queue<linked_ptr<PendingOperation> > operation_queue_;

  // A set of ids that have been provisionally added, which we're willing to
  // consider allowed until we hear back from the server signature request.
  ExtensionIdSet provisional_;

  DISALLOW_COPY_AND_ASSIGN(InstallVerifier);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_VERIFIER_H_
