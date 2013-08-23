// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_PLATFORM_VERIFICATION_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_PLATFORM_VERIFICATION_FLOW_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}

namespace cryptohome {
class AsyncMethodCaller;
}

namespace chromeos {

class CryptohomeClient;
class UserManager;

namespace attestation {

class AttestationFlow;

// This class allows platform verification for the content protection use case.
// All methods must only be called on the UI thread.  Example:
//   PlatformVerificationFlow verifier;
//   PlatformVerificationFlow::Callback callback = base::Bind(&MyCallback);
//   verifier.ChallengePlatformKey(my_web_contents, "my_id", "some_challenge",
//                                 callback);
class PlatformVerificationFlow {
 public:
  enum Result {
    SUCCESS,                // The operation succeeded.
    INTERNAL_ERROR,         // The operation failed unexpectedly.
    PLATFORM_NOT_VERIFIED,  // The platform cannot be verified.  For example:
                            // - It is not a Chrome device.
                            // - It is not running a verified OS image.
    USER_REJECTED,          // The user explicitly rejected the operation.
    POLICY_REJECTED,        // The operation is not allowed by policy/settings.
  };

  enum ConsentType {
    CONSENT_TYPE_NONE,         // No consent necessary.
    CONSENT_TYPE_ATTESTATION,  // Consent to use attestation.
    CONSENT_TYPE_ORIGIN,       // Consent to proceed with an unfamiliar origin.
    CONSENT_TYPE_ALWAYS,       // Consent because 'Always Ask' was requested.
  };

  enum ConsentResponse {
    CONSENT_RESPONSE_NONE,
    CONSENT_RESPONSE_ALLOW,
    CONSENT_RESPONSE_DENY,
    CONSENT_RESPONSE_ALWAYS_ASK,
  };

  // An interface which allows settings and UI to be abstracted for testing
  // purposes.  For normal operation the default implementation should be used.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // This callback will be called when a user has given a |response| to a
    // consent request of the specified |type|.
    typedef base::Callback<void(ConsentResponse response)> ConsentCallback;

    // Invokes consent UI of the given |type| within the context of
    // |web_contents| and calls |callback| when the user responds.
    virtual void ShowConsentPrompt(ConsentType type,
                                   content::WebContents* web_contents,
                                   const ConsentCallback& callback) = 0;

    // Returns true if settings indicate that attestation should be disabled.
    virtual bool IsAttestationDisabled() = 0;

    // Checks if the web origin represented by |web_contents| is unfamiliar and
    // requires special user consent.
    virtual bool IsOriginConsentRequired(
        content::WebContents* web_contents) = 0;

    // Checks if settings indicate that consent is required for the web origin
    // represented by |web_contents| because the user requested to be prompted.
    virtual bool IsAlwaysAskRequired(content::WebContents* web_contents) = 0;

    // Updates user settings based on their response to the consent request.
    virtual bool UpdateSettings(content::WebContents* web_contents,
                                ConsentType consent_type,
                                ConsentResponse consent_response) = 0;
  };

  // This callback will be called when a challenge operation completes.  If
  // |result| is SUCCESS then |challenge_response| holds the challenge response
  // as specified by the protocol.  The |platform_key_certificate| is for the
  // key which was used to create the challenge response.  This key may be
  // generated on demand and is not guaranteed to persist across multiple calls
  // to this method.  Both the response and the certificate are opaque to
  // the browser; they are intended for validation by an external application or
  // service.
  typedef base::Callback<void(Result result,
                              const std::string& challenge_response,
                              const std::string& platform_key_certificate)>
      ChallengeCallback;

  // A constructor that uses the default implementation of all dependencies
  // including Delegate.
  // TODO(dkrahn): Enable this when the default delegate has been implemented.
  // PlatformVerificationFlow();

  // An alternate constructor which specifies dependent objects explicitly.
  // This is useful in testing.  The caller retains ownership of all pointers.
  PlatformVerificationFlow(AttestationFlow* attestation_flow,
                           cryptohome::AsyncMethodCaller* async_caller,
                           CryptohomeClient* cryptohome_client,
                           UserManager* user_manager,
                           Delegate* delegate);

  virtual ~PlatformVerificationFlow();

  // Invokes an asynchronous operation to challenge a platform key.  Any user
  // interaction will be associated with |web_contents|.  The |service_id| is an
  // arbitrary value but it should uniquely identify the origin of the request
  // and should not be determined by that origin; its purpose is to prevent
  // collusion between multiple services.  The |challenge| is also an arbitrary
  // value but it should be time sensitive or associated to some kind of session
  // because its purpose is to prevent certificate replay.  The |callback| will
  // be called when the operation completes.  The duration of the operation can
  // vary depending on system state, hardware capabilities, and interaction with
  // the user.
  void ChallengePlatformKey(content::WebContents* web_contents,
                            const std::string& service_id,
                            const std::string& challenge,
                            const ChallengeCallback& callback);

 private:
  // Checks whether we need to prompt the user for consent before proceeding and
  // invokes the consent UI if so.  All parameters are the same as in
  // ChallengePlatformKey except for the additional |attestation_enrolled| which
  // specifies whether attestation has been enrolled for this device.
  void CheckConsent(content::WebContents* web_contents,
                    const std::string& service_id,
                    const std::string& challenge,
                    const ChallengeCallback& callback,
                    bool attestation_enrolled);

  // A callback called when the user has given their consent response.  All
  // parameters are the same as in ChallengePlatformKey except for the
  // additional |consent_type| and |consent_response| which indicate the consent
  // type and user response, respectively.  If the response indicates that the
  // operation should proceed, this method invokes a certificate request.
  void OnConsentResponse(content::WebContents* web_contents,
                         const std::string& service_id,
                         const std::string& challenge,
                         const ChallengeCallback& callback,
                         ConsentType consent_type,
                         ConsentResponse consent_response);

  // A callback called when an attestation certificate request operation
  // completes.  |service_id|, |challenge|, and |callback| are the same as in
  // ChallengePlatformKey.  |operation_success| is true iff the certificate
  // request operation succeeded.  |certificate| holds the certificate for the
  // platform key on success.  If the certificate request was successful, this
  // method invokes a request to sign the challenge.
  void OnCertificateReady(const std::string& service_id,
                          const std::string& challenge,
                          const ChallengeCallback& callback,
                          bool operation_success,
                          const std::string& certificate);

  // A callback called when a challenge signing request has completed.  The
  // |certificate| is the platform certificate for the key which signed the
  // challenge.  |callback| is the same as in ChallengePlatformKey.
  // |operation_success| is true iff the challenge signing operation was
  // successful.  If it was successful, |response_data| holds the challenge
  // response and the method will invoke |callback|.
  void OnChallengeReady(const std::string& certificate,
                        const ChallengeCallback& callback,
                        bool operation_success,
                        const std::string& response_data);

  AttestationFlow* attestation_flow_;
  scoped_ptr<AttestationFlow> default_attestation_flow_;
  cryptohome::AsyncMethodCaller* async_caller_;
  CryptohomeClient* cryptohome_client_;
  UserManager* user_manager_;
  Delegate* delegate_;
  scoped_ptr<Delegate> default_delegate_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<PlatformVerificationFlow> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformVerificationFlow);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_PLATFORM_VERIFICATION_FLOW_H_
