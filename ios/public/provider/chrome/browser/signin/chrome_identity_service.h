// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_SERVICE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"

@class ChromeIdentity;
@class NSArray;
@class NSDate;
@class NSError;
@class NSString;
@class UIImage;

namespace ios {

// Callback passed to method |SigninIdentity()|.
typedef void (^SigninIdentityCallback)(ChromeIdentity* identity,
                                       NSError* error);

// Callback passed to method |GetAccessTokenForScopes()| that returns the
// information of the obtained access token to the caller.
typedef void (^AccessTokenCallback)(NSString* token,
                                    NSDate* expiration,
                                    NSError* error);

// Callback passed to method |ForgetIdentity()|. |error| is nil if the operation
// completed with success.
typedef void (^ForgetIdentityCallback)(NSError* error);

// Callback passed to method |GetAvatarForIdentity()|.
typedef void (^GetAvatarCallback)(UIImage* avatar);

// Describes the reason for an access token error.
enum class AccessTokenErrorReason { INVALID_GRANT, UNKNOWN_ERROR };

// ChromeIdentityService abstracts the signin flow on iOS.
class ChromeIdentityService {
 public:
  // Observer handling events related to the ChromeIdentityService.
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    // Handles identity list changed events.
    virtual void OnIdentityListChanged() {}

    // Handles access token refresh failed events.
    // |identity| is the the identity for which the access token refresh failed.
    virtual void OnAccessTokenRefreshFailed(ChromeIdentity* identity,
                                            AccessTokenErrorReason error) {}

    // Called when profile information or the profile image is updated.
    virtual void OnProfileUpdate(ChromeIdentity* identity) {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  ChromeIdentityService();
  virtual ~ChromeIdentityService();

  // Returns YES if |identity| is valid and if the service has it in its list of
  // identitites.
  virtual bool IsValidIdentity(ChromeIdentity* identity) const;

  // Returns the chrome identity having the email equal to |email| or |nil| if
  // no matching identity is found.
  virtual ChromeIdentity* GetIdentityWithEmail(const std::string& email) const;

  // Returns the chrome identity having the gaia ID equal to |gaia_id| or |nil|
  // if no matching identity is found.
  virtual ChromeIdentity* GetIdentityWithGaiaID(
      const std::string& gaia_id) const;

  // Returns the canonicalized emails for all identities.
  virtual std::vector<std::string> GetCanonicalizeEmailsForAllIdentities()
      const;

  // Returns true if there is at least one identity.
  virtual bool HasIdentities() const;

  // Returns all ChromeIdentity objects in an array.
  virtual NSArray* GetAllIdentities() const;

  // Returns all ChromeIdentity objects sorted by the ordering used in the
  // account manager, which is typically based on the keychain ordering of
  // accounts.
  virtual NSArray* GetAllIdentitiesSortedForDisplay() const;

  // Forgets the given identity on the device. This method logs the user out.
  // It is asynchronous because it needs to contact the server to revoke the
  // authentication token.
  // This may be called on an arbitrary thread, but callback will always be on
  // the main thread.
  virtual void ForgetIdentity(ChromeIdentity* identity,
                              ForgetIdentityCallback callback);

  // Asynchronously retrieves access tokens for the given identity and scopes.
  // Uses the default client id and client secret.
  virtual void GetAccessToken(ChromeIdentity* identity,
                              const std::set<std::string>& scopes,
                              const AccessTokenCallback& callback);

  // Asynchronously retrieves access tokens for the given identity and scopes.
  virtual void GetAccessToken(ChromeIdentity* identity,
                              const std::string& client_id,
                              const std::string& client_secret,
                              const std::set<std::string>& scopes,
                              const AccessTokenCallback& callback);

  // Allow the user to sign in with an identity already seen on this device.
  virtual void SigninIdentity(ChromeIdentity* identity,
                              SigninIdentityCallback callback);

  // Fetches the profile avatar, from the cache or the network.
  // For high resolution iPads, returns large images (200 x 200) to avoid
  // pixelization. Calls back on the main thread.
  virtual void GetAvatarForIdentity(ChromeIdentity* identity,
                                    GetAvatarCallback callback);

  // Synchronously returns any cached avatar, or nil.
  // GetAvatarForIdentity() should be generally used instead of this method.
  virtual UIImage* GetCachedAvatarForIdentity(ChromeIdentity* identity);

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // Fires |OnIdentityListChanged| on all observers.
  void FireIdentityListChanged();

  // Fires |OnAccessTokenRefreshFailed| on all observers, with the corresponding
  // identity and error reason.
  void FireAccessTokenRefreshFailed(ChromeIdentity* identity,
                                    AccessTokenErrorReason error);

  // Fires |OnProfileUpdate| on all observers, with the corresponding identity.
  void FireProfileDidUpdate(ChromeIdentity* identity);

 private:
  base::ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ChromeIdentityService);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_SERVICE_H_
