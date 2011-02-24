// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service_mac.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/scoped_cftyperef.h"

using base::TimeDelta;
using base::TimeTicks;

namespace net {

namespace {

static const int kConfigUpdateInterval = 10;  // seconds

static const bool kSSL3EnabledDefaultValue = true;
static const bool kTLS1EnabledDefaultValue = true;

static CFStringRef kRevocationPreferencesIdentifier =
    CFSTR("com.apple.security.revocation");
static CFStringRef kOCSPStyleKey = CFSTR("OCSPStyle");
static CFStringRef kCRLStyleKey = CFSTR("CRLStyle");
static CFStringRef kNoneRevocationValue = CFSTR("None");
static CFStringRef kBestAttemptRevocationValue = CFSTR("BestAttempt");
static CFStringRef kSSL3EnabledKey = CFSTR("org.chromium.ssl.ssl3");
static CFStringRef kTLS1EnabledKey = CFSTR("org.chromium.ssl.tls1");

bool RevocationStyleIsEnabled(CFStringRef key) {
  CFPropertyListRef plist_ref = CFPreferencesCopyValue(key,
      kRevocationPreferencesIdentifier, kCFPreferencesCurrentUser,
      kCFPreferencesAnyHost);
  if (plist_ref) {
    base::mac::ScopedCFTypeRef<CFPropertyListRef> scoped_plist_ref(plist_ref);
    if (CFGetTypeID(plist_ref) == CFStringGetTypeID()) {
      CFStringRef style = reinterpret_cast<CFStringRef>(plist_ref);
      if (CFStringCompare(kNoneRevocationValue, style,
                          kCFCompareCaseInsensitive))
        return true;
    }
  }
  return false;
}

inline bool SSLVersionIsEnabled(CFStringRef key, bool default_value) {
  Boolean exists_and_valid;
  Boolean rv = CFPreferencesGetAppBooleanValue(key,
                                               kCFPreferencesCurrentApplication,
                                               &exists_and_valid);
  if (!exists_and_valid)
    return default_value;
  return rv;
}

}  // namespace

SSLConfigServiceMac::SSLConfigServiceMac() : ever_updated_(false) {
  // We defer retrieving the settings until the first call to GetSSLConfig, to
  // avoid an expensive call on the UI thread, which could affect startup time.
}

SSLConfigServiceMac::SSLConfigServiceMac(TimeTicks now) : ever_updated_(false) {
  UpdateConfig(now);
}

void SSLConfigServiceMac::GetSSLConfig(SSLConfig* config) {
  GetSSLConfigAt(config, base::TimeTicks::Now());
}

void SSLConfigServiceMac::GetSSLConfigAt(SSLConfig* config, TimeTicks now) {
  if (!ever_updated_ ||
      now - config_time_ > TimeDelta::FromSeconds(kConfigUpdateInterval))
    UpdateConfig(now);
  *config = config_info_;
}

SSLConfigServiceMac::~SSLConfigServiceMac() {}

// static
bool SSLConfigServiceMac::GetSSLConfigNow(SSLConfig* config) {
  // Our own revocation checking flag is a binary value, but Mac OS X uses
  // several shades of revocation checking:
  //   - None (i.e., disabled, the default)
  //   - BestAttempt
  //   - RequireIfPresent
  //   - RequireForall
  // Mac OS X also breaks down revocation check for both CRLs and OCSP. We
  // set our revocation flag if the system-wide settings for either OCSP
  // or CRLs is anything other than None.
  config->rev_checking_enabled = (RevocationStyleIsEnabled(kOCSPStyleKey) ||
                                  RevocationStyleIsEnabled(kCRLStyleKey));

  config->ssl3_enabled = SSLVersionIsEnabled(kSSL3EnabledKey,
                                             kSSL3EnabledDefaultValue);
  config->tls1_enabled = SSLVersionIsEnabled(kTLS1EnabledKey,
                                             kTLS1EnabledDefaultValue);
  SSLConfigService::SetSSLConfigFlags(config);

  // TODO(rsleevi): http://crbug.com/58831 - Implement preferences for
  // disabling cipher suites.
  return true;
}

// static
void SSLConfigServiceMac::SetSSL3Enabled(bool enabled) {
  CFPreferencesSetAppValue(kSSL3EnabledKey,
                           enabled ? kCFBooleanTrue : kCFBooleanFalse,
                           kCFPreferencesCurrentApplication);
  CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}

// static
void SSLConfigServiceMac::SetTLS1Enabled(bool enabled) {
  CFPreferencesSetAppValue(kTLS1EnabledKey,
                           enabled ? kCFBooleanTrue : kCFBooleanFalse,
                           kCFPreferencesCurrentApplication);
  CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}

// static
void SSLConfigServiceMac::SetRevCheckingEnabled(bool enabled) {
  // This method is provided for use by the unit tests. These settings
  // are normally changed via the Keychain Access application's preferences
  // dialog.
  CFPreferencesSetValue(kOCSPStyleKey,
      enabled ? kBestAttemptRevocationValue : kNoneRevocationValue,
      kRevocationPreferencesIdentifier, kCFPreferencesCurrentUser,
      kCFPreferencesAnyHost);
  CFPreferencesSetValue(kCRLStyleKey,
      enabled ? kBestAttemptRevocationValue : kNoneRevocationValue,
      kRevocationPreferencesIdentifier, kCFPreferencesCurrentUser,
      kCFPreferencesAnyHost);
}

void SSLConfigServiceMac::UpdateConfig(TimeTicks now) {
  SSLConfig orig_config = config_info_;
  GetSSLConfigNow(&config_info_);
  if (ever_updated_)
    ProcessConfigUpdate(orig_config, config_info_);
  config_time_ = now;
  ever_updated_ = true;
}

}  // namespace net
