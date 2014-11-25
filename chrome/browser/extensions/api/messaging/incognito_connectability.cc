// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

IncognitoConnectability::ScopedAlertTracker::Mode g_alert_mode =
    IncognitoConnectability::ScopedAlertTracker::INTERACTIVE;
int g_alert_count = 0;

class IncognitoConnectabilityInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  typedef base::Callback<void(
      IncognitoConnectability::ScopedAlertTracker::Mode)> InfoBarCallback;

  // Creates a confirmation infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const base::string16& message,
                     const InfoBarCallback& callback) {
    infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
        scoped_ptr<ConfirmInfoBarDelegate>(
            new IncognitoConnectabilityInfoBarDelegate(message, callback))));
  }

 private:
  IncognitoConnectabilityInfoBarDelegate(const base::string16& message,
                                         const InfoBarCallback& callback)
      : message_(message), answered_(false), callback_(callback) {}

  ~IncognitoConnectabilityInfoBarDelegate() override {
    if (!answered_) {
      // The infobar has closed without the user expressing an explicit
      // preference. The current request should be denied but further requests
      // should show an interactive prompt.
      callback_.Run(IncognitoConnectability::ScopedAlertTracker::INTERACTIVE);
    }
  }

  // ConfirmInfoBarDelegate:
  base::string16 GetMessageText() const override { return message_; }
  base::string16 GetButtonLabel(InfoBarButton button) const override {
    return l10n_util::GetStringUTF16(
        (button == BUTTON_OK) ? IDS_PERMISSION_ALLOW : IDS_PERMISSION_DENY);
  }

  bool Accept() override {
    callback_.Run(IncognitoConnectability::ScopedAlertTracker::ALWAYS_ALLOW);
    answered_ = true;
    return true;
  }
  bool Cancel() override {
    callback_.Run(IncognitoConnectability::ScopedAlertTracker::ALWAYS_DENY);
    answered_ = true;
    return true;
  }

  // InfoBarDelegate:
  Type GetInfoBarType() const override { return PAGE_ACTION_TYPE; }

  base::string16 message_;
  bool answered_;
  InfoBarCallback callback_;
};

}  // namespace

IncognitoConnectability::ScopedAlertTracker::ScopedAlertTracker(Mode mode)
    : last_checked_invocation_count_(g_alert_count) {
  DCHECK_EQ(INTERACTIVE, g_alert_mode);
  DCHECK_NE(INTERACTIVE, mode);
  g_alert_mode = mode;
}

IncognitoConnectability::ScopedAlertTracker::~ScopedAlertTracker() {
  DCHECK_NE(INTERACTIVE, g_alert_mode);
  g_alert_mode = INTERACTIVE;
}

int IncognitoConnectability::ScopedAlertTracker::GetAndResetAlertCount() {
  int result = g_alert_count - last_checked_invocation_count_;
  last_checked_invocation_count_ = g_alert_count;
  return result;
}

IncognitoConnectability::IncognitoConnectability(
    content::BrowserContext* context)
    : weak_factory_(this) {
  CHECK(context->IsOffTheRecord());
}

IncognitoConnectability::~IncognitoConnectability() {
}

// static
IncognitoConnectability* IncognitoConnectability::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<IncognitoConnectability>::Get(context);
}

void IncognitoConnectability::Query(
    const Extension* extension,
    content::WebContents* web_contents,
    const GURL& url,
    const base::Callback<void(bool)>& callback) {
  GURL origin = url.GetOrigin();
  if (origin.is_empty()) {
    callback.Run(false);
    return;
  }

  if (IsInMap(extension, origin, allowed_origins_)) {
    callback.Run(true);
    return;
  }

  if (IsInMap(extension, origin, disallowed_origins_)) {
    callback.Run(false);
    return;
  }

  ExtensionOriginPair extension_origin_pair =
      ExtensionOriginPair(extension->id(), origin);
  if (ContainsKey(pending_origins_, extension_origin_pair)) {
    // We are already asking the user.
    pending_origins_[extension_origin_pair].push_back(callback);
    return;
  }

  // We need to ask the user.
  ++g_alert_count;
  pending_origins_[extension_origin_pair].push_back(callback);

  switch (g_alert_mode) {
    // Production code should always be using INTERACTIVE.
    case ScopedAlertTracker::INTERACTIVE: {
      int template_id =
          extension->is_app()
              ? IDS_EXTENSION_PROMPT_APP_CONNECT_FROM_INCOGNITO
              : IDS_EXTENSION_PROMPT_EXTENSION_CONNECT_FROM_INCOGNITO;
      IncognitoConnectabilityInfoBarDelegate::Create(
          InfoBarService::FromWebContents(web_contents),
          l10n_util::GetStringFUTF16(template_id,
                                     base::UTF8ToUTF16(origin.spec()),
                                     base::UTF8ToUTF16(extension->name())),
          base::Bind(&IncognitoConnectability::OnInteractiveResponse,
                     weak_factory_.GetWeakPtr(), extension->id(), origin));
      break;
    }

    // Testing code can override to always allow or deny.
    case ScopedAlertTracker::ALWAYS_ALLOW:
    case ScopedAlertTracker::ALWAYS_DENY:
      OnInteractiveResponse(extension->id(), origin, g_alert_mode);
      break;
  }
}

void IncognitoConnectability::OnInteractiveResponse(
    const std::string& extension_id,
    const GURL& origin,
    ScopedAlertTracker::Mode response) {
  switch (response) {
    case ScopedAlertTracker::ALWAYS_ALLOW:
      allowed_origins_[extension_id].insert(origin);
      break;
    case ScopedAlertTracker::ALWAYS_DENY:
      disallowed_origins_[extension_id].insert(origin);
      break;
    default:
      // Otherwise the user has not expressed an explicit preference and so
      // nothing should be permanently recorded.
      break;
  }

  ExtensionOriginPair extension_origin_pair =
      ExtensionOriginPair(extension_id, origin);
  PendingAuthorizationMap::iterator pending_origin =
      pending_origins_.find(extension_origin_pair);
  DCHECK(pending_origin != pending_origins_.end());

  std::vector<AuthorizationCallback> callbacks;
  callbacks.swap(pending_origin->second);
  pending_origins_.erase(pending_origin);
  DCHECK(!callbacks.empty());

  for (const AuthorizationCallback& callback : callbacks) {
    callback.Run(response == ScopedAlertTracker::ALWAYS_ALLOW);
  }
}

bool IncognitoConnectability::IsInMap(const Extension* extension,
                                      const GURL& origin,
                                      const ExtensionToOriginsMap& map) {
  DCHECK_EQ(origin, origin.GetOrigin());
  ExtensionToOriginsMap::const_iterator it = map.find(extension->id());
  return it != map.end() && it->second.count(origin) > 0;
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<IncognitoConnectability> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<IncognitoConnectability>*
IncognitoConnectability::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
