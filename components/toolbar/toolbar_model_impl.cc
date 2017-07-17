// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/toolbar/toolbar_model_impl.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/toolbar/features.h"
#include "components/toolbar/toolbar_model_delegate.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/vector_icon_types.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#include "components/toolbar/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"  // nogncheck
#endif

ToolbarModelImpl::ToolbarModelImpl(ToolbarModelDelegate* delegate,
                                   size_t max_url_display_chars)
    : delegate_(delegate), max_url_display_chars_(max_url_display_chars) {
  DCHECK(delegate_);
}

ToolbarModelImpl::~ToolbarModelImpl() {
}

// ToolbarModelImpl Implementation.
base::string16 ToolbarModelImpl::GetFormattedURL(size_t* prefix_end) const {
  GURL url(GetURL());
  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  const base::string16 formatted_text =
      delegate_->FormattedStringWithEquivalentMeaning(
          url, url_formatter::FormatUrl(
                   url, url_formatter::kFormatUrlOmitAll,
                   net::UnescapeRule::NORMAL, nullptr, prefix_end, nullptr));
  if (formatted_text.length() <= max_url_display_chars_)
    return formatted_text;

  // Truncating the URL breaks editing and then pressing enter, but hopefully
  // people won't try to do much with such enormous URLs anyway. If this becomes
  // a real problem, we could perhaps try to keep some sort of different "elided
  // visible URL" where editing affects and reloads the "real underlying URL",
  // but this seems very tricky for little gain.
  return gfx::TruncateString(formatted_text, max_url_display_chars_ - 1,
                             gfx::CHARACTER_BREAK) +
         gfx::kEllipsisUTF16;
}

GURL ToolbarModelImpl::GetURL() const {
  GURL url;
  return delegate_->GetURL(&url) ? url : GURL(url::kAboutBlankURL);
}

security_state::SecurityLevel ToolbarModelImpl::GetSecurityLevel(
    bool ignore_editing) const {
  // When editing or empty, assume no security style.
  return ((input_in_progress() && !ignore_editing) || !ShouldDisplayURL())
             ? security_state::NONE
             : delegate_->GetSecurityLevel();
}

const gfx::VectorIcon& ToolbarModelImpl::GetVectorIcon() const {
#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
  auto* const icon_override = delegate_->GetVectorIconOverride();
  if (icon_override)
    return *icon_override;

  if (IsOfflinePage())
    return toolbar::kOfflinePinIcon;

  switch (GetSecurityLevel(false)) {
    case security_state::NONE:
    case security_state::HTTP_SHOW_WARNING:
      return toolbar::kHttpIcon;
    case security_state::EV_SECURE:
    case security_state::SECURE:
      return toolbar::kHttpsValidIcon;
    case security_state::SECURITY_WARNING:
      // Surface Dubious as Neutral.
      return toolbar::kHttpIcon;
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      return vector_icons::kBusinessIcon;
    case security_state::DANGEROUS:
      return toolbar::kHttpsInvalidIcon;
  }
  NOTREACHED();
  return toolbar::kHttpIcon;
#else
  NOTREACHED();
  static const gfx::VectorIcon dummy = {};
  return dummy;
#endif
}

base::string16 ToolbarModelImpl::GetEVCertName() const {
  if (GetSecurityLevel(false) != security_state::EV_SECURE)
    return base::string16();

  // Note: cert is guaranteed non-NULL or the security level would be NONE.
  scoped_refptr<net::X509Certificate> cert = delegate_->GetCertificate();
  DCHECK(cert.get());

  // EV are required to have an organization name and country.
  DCHECK(!cert->subject().organization_names.empty());
  DCHECK(!cert->subject().country_name.empty());
  return l10n_util::GetStringFUTF16(
      IDS_SECURE_CONNECTION_EV,
      base::UTF8ToUTF16(cert->subject().organization_names[0]),
      base::UTF8ToUTF16(cert->subject().country_name));
}

base::string16 ToolbarModelImpl::GetSecureVerboseText() const {
  if (IsOfflinePage())
    return l10n_util::GetStringUTF16(IDS_OFFLINE_VERBOSE_STATE);

  switch (GetSecurityLevel(false)) {
    case security_state::HTTP_SHOW_WARNING:
      return l10n_util::GetStringUTF16(IDS_NOT_SECURE_VERBOSE_STATE);
    case security_state::SECURE:
      return l10n_util::GetStringUTF16(IDS_SECURE_VERBOSE_STATE);
    case security_state::DANGEROUS:
      return l10n_util::GetStringUTF16(delegate_->FailsMalwareCheck()
                                           ? IDS_DANGEROUS_VERBOSE_STATE
                                           : IDS_NOT_SECURE_VERBOSE_STATE);
    default:
      return base::string16();
  }
}

bool ToolbarModelImpl::ShouldDisplayURL() const {
  return delegate_->ShouldDisplayURL();
}

bool ToolbarModelImpl::IsOfflinePage() const {
  return delegate_->IsOfflinePage();
}
