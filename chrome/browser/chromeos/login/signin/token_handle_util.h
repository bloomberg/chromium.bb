// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_TOKEN_HANDLE_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_TOKEN_HANDLE_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/weak_ptr.h"
#include "components/user_manager/user_id.h"
#include "google_apis/gaia/gaia_oauth_client.h"

namespace base {
class DictionaryValue;
}

namespace user_manager {
class UserManager;
}

// This class is responsible for operations with External Token Handle.
// Handle is an extra token associated with OAuth refresh token that have
// exactly same lifetime. It is not secure, and it's only purpose is checking
// validity of corresponding refresh token in the insecure environment.
class TokenHandleUtil {
 public:
  explicit TokenHandleUtil(user_manager::UserManager* user_manager);
  ~TokenHandleUtil();

  enum TokenHandleStatus { VALID, INVALID, UNKNOWN };

  typedef base::Callback<void(const user_manager::UserID&, TokenHandleStatus)>
      TokenValidationCallback;

  // Returns true if UserManager has token handle associated with |user_id|.
  bool HasToken(const user_manager::UserID& user_id);

  // Removes token handle for |user_id| from UserManager storage.
  void DeleteHandle(const user_manager::UserID& user_id);

  // Marks current handle as invalid, new one should be obtained at next sign
  // in.
  void MarkHandleInvalid(const user_manager::UserID& user_id);

  // Indicates if token handle for |user_id| is missing or marked as invalid.
  bool ShouldObtainHandle(const user_manager::UserID& user_id);

  // Performs token handle check for |user_id|. Will call |callback| with
  // corresponding result.
  void CheckToken(const user_manager::UserID& user_id,
                  const TokenValidationCallback& callback);

  // Given the OAuth |access_token| attempt to obtain token handle and store it
  // for |user_id|.
  void GetTokenHandle(const user_manager::UserID& user_id,
                      const std::string& access_token,
                      const TokenValidationCallback& callback);

 private:
  // Associates GaiaOAuthClient::Delegate with User ID and Token.
  class TokenDelegate : public gaia::GaiaOAuthClient::Delegate {
   public:
    TokenDelegate(const base::WeakPtr<TokenHandleUtil>& owner,
                  bool obtain,
                  const user_manager::UserID& user_id,
                  const std::string& token,
                  const TokenValidationCallback& callback);
    ~TokenDelegate() override;
    void OnOAuthError() override;
    void OnNetworkError(int response_code) override;
    void OnGetTokenInfoResponse(
        scoped_ptr<base::DictionaryValue> token_info) override;
    void NotifyDone();

   private:
    base::WeakPtr<TokenHandleUtil> owner_;
    bool obtain_;
    user_manager::UserID user_id_;
    std::string token_;
    TokenValidationCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(TokenDelegate);
  };

  void OnValidationComplete(const std::string& token);
  void OnObtainTokenComplete(const user_manager::UserID& id);
  void StoreTokenHandle(const user_manager::UserID& user_id,
                        const std::string& handle);

  // UserManager that stores corresponding user data.
  user_manager::UserManager* user_manager_;

  // Map of pending check operations.
  base::ScopedPtrHashMap<std::string, scoped_ptr<TokenDelegate>>
      validation_delegates_;

  // Map of pending obtain operations.
  base::ScopedPtrHashMap<user_manager::UserID, scoped_ptr<TokenDelegate>>
      obtain_delegates_;

  // Instance of GAIA Client.
  scoped_ptr<gaia::GaiaOAuthClient> gaia_client_;

  base::WeakPtrFactory<TokenHandleUtil> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TokenHandleUtil);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SIGNIN_TOKEN_HANDLE_UTIL_H_
