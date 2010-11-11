// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service_win.h"

#include "base/thread_restrictions.h"
#include "base/win/registry.h"

using base::TimeDelta;
using base::TimeTicks;
using base::win::RegKey;

namespace net {

static const int kConfigUpdateInterval = 10;  // seconds

static const wchar_t kInternetSettingsSubKeyName[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";

static const wchar_t kRevocationValueName[] = L"CertificateRevocation";

static const wchar_t kProtocolsValueName[] = L"SecureProtocols";

// In SecureProtocols, each SSL version is represented by a bit:
//   SSL 2.0: 0x08
//   SSL 3.0: 0x20
//   TLS 1.0: 0x80
// The bits are OR'ed to form the DWORD value.  So 0xa0 means SSL 3.0 and
// TLS 1.0.
enum {
  SSL2 = 0x08,
  SSL3 = 0x20,
  TLS1 = 0x80
};

// If CertificateRevocation or SecureProtocols is missing, IE uses a default
// value.  Unfortunately the default is IE version specific.  We use WinHTTP's
// default.
enum {
  REVOCATION_DEFAULT = 0,
  PROTOCOLS_DEFAULT = SSL3 | TLS1
};

SSLConfigServiceWin::SSLConfigServiceWin() : ever_updated_(false) {
  // We defer retrieving the settings until the first call to GetSSLConfig, to
  // avoid an expensive call on the UI thread, which could affect startup time.
}

SSLConfigServiceWin::SSLConfigServiceWin(TimeTicks now) : ever_updated_(false) {
  UpdateConfig(now);
}

void SSLConfigServiceWin::GetSSLConfigAt(SSLConfig* config, TimeTicks now) {
  if (!ever_updated_ ||
      now - config_time_ > TimeDelta::FromSeconds(kConfigUpdateInterval))
    UpdateConfig(now);
  *config = config_info_;
}

// static
bool SSLConfigServiceWin::GetSSLConfigNow(SSLConfig* config) {
  // This registry access goes to disk and will slow down the IO thread.
  // http://crbug.com/61455
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  RegKey internet_settings;
  if (!internet_settings.Open(HKEY_CURRENT_USER, kInternetSettingsSubKeyName,
                              KEY_READ))
    return false;

  DWORD revocation;
  if (!internet_settings.ReadValueDW(kRevocationValueName, &revocation))
    revocation = REVOCATION_DEFAULT;

  DWORD protocols;
  if (!internet_settings.ReadValueDW(kProtocolsValueName, &protocols))
    protocols = PROTOCOLS_DEFAULT;

  config->rev_checking_enabled = (revocation != 0);
  config->ssl2_enabled = ((protocols & SSL2) != 0);
  config->ssl3_enabled = ((protocols & SSL3) != 0);
  config->tls1_enabled = ((protocols & TLS1) != 0);
  SSLConfigService::SetSSLConfigFlags(config);

  // TODO(rsleevi): Possibly respect the registry keys defined in
  // http://support.microsoft.com/kb/245030 (pre-Vista) or
  // http://msdn.microsoft.com/en-us/library/bb870930(VS.85).aspx (post-Vista).
  // Currently, these values are respected implicitly when using
  // SSLClientSocketWin, but they do not propogate to SSLClientSocketNSS
  // because we're not currently translating the keys.

  return true;
}

// static
void SSLConfigServiceWin::SetRevCheckingEnabled(bool enabled) {
  // This registry access goes to disk and will slow down the IO thread.
  // http://crbug.com/61455
  DWORD value = enabled;
  RegKey internet_settings(HKEY_CURRENT_USER, kInternetSettingsSubKeyName,
                           KEY_WRITE);
  internet_settings.WriteValue(kRevocationValueName, value);
  // TODO(mattm): We should call UpdateConfig after updating settings, but these
  // methods are static.
}

// static
void SSLConfigServiceWin::SetSSL2Enabled(bool enabled) {
  SetSSLVersionEnabled(SSL2, enabled);
}

// static
void SSLConfigServiceWin::SetSSL3Enabled(bool enabled) {
  SetSSLVersionEnabled(SSL3, enabled);
}

// static
void SSLConfigServiceWin::SetTLS1Enabled(bool enabled) {
  SetSSLVersionEnabled(TLS1, enabled);
}

// static
void SSLConfigServiceWin::SetSSLVersionEnabled(int version, bool enabled) {
  // This registry access goes to disk and will slow down the IO thread.
  // http://crbug.com/61455
  RegKey internet_settings(HKEY_CURRENT_USER, kInternetSettingsSubKeyName,
                           KEY_READ | KEY_WRITE);
  DWORD value;
  if (!internet_settings.ReadValueDW(kProtocolsValueName, &value))
    value = PROTOCOLS_DEFAULT;
  if (enabled)
    value |= version;
  else
    value &= ~version;
  internet_settings.WriteValue(kProtocolsValueName, value);
  // TODO(mattm): We should call UpdateConfig after updating settings, but these
  // methods are static.
}

void SSLConfigServiceWin::UpdateConfig(TimeTicks now) {
  SSLConfig orig_config = config_info_;
  GetSSLConfigNow(&config_info_);
  if (ever_updated_)
    ProcessConfigUpdate(orig_config, config_info_);
  config_time_ = now;
  ever_updated_ = true;
}

}  // namespace net
