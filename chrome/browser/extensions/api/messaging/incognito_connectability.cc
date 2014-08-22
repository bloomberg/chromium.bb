// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {
IncognitoConnectability::ScopedAlertTracker::Mode g_alert_mode =
    IncognitoConnectability::ScopedAlertTracker::INTERACTIVE;
int g_alert_count = 0;
}

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
    content::BrowserContext* context) {
  CHECK(context->IsOffTheRecord());
}

IncognitoConnectability::~IncognitoConnectability() {
}

// static
IncognitoConnectability* IncognitoConnectability::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<IncognitoConnectability>::Get(context);
}

bool IncognitoConnectability::Query(const Extension* extension,
                                    content::WebContents* web_contents,
                                    const GURL& url) {
  GURL origin = url.GetOrigin();
  if (origin.is_empty())
    return false;

  if (IsInMap(extension, origin, allowed_origins_))
    return true;
  if (IsInMap(extension, origin, disallowed_origins_))
    return false;

  // We need to ask the user.
  ++g_alert_count;
  chrome::MessageBoxResult result = chrome::MESSAGE_BOX_RESULT_NO;

  switch (g_alert_mode) {
    // Production code should always be using INTERACTIVE.
    case ScopedAlertTracker::INTERACTIVE: {
      int template_id = extension->is_app() ?
          IDS_EXTENSION_PROMPT_APP_CONNECT_FROM_INCOGNITO :
          IDS_EXTENSION_PROMPT_EXTENSION_CONNECT_FROM_INCOGNITO;
      result = chrome::ShowMessageBox(
          web_contents ? web_contents->GetTopLevelNativeWindow() : NULL,
          base::string16(),  // no title
          l10n_util::GetStringFUTF16(template_id,
                                     base::UTF8ToUTF16(origin.spec()),
                                     base::UTF8ToUTF16(extension->name())),
          chrome::MESSAGE_BOX_TYPE_QUESTION);
      break;
    }

    // Testing code can override to always allow or deny.
    case ScopedAlertTracker::ALWAYS_ALLOW:
      result = chrome::MESSAGE_BOX_RESULT_YES;
      break;
    case ScopedAlertTracker::ALWAYS_DENY:
      result = chrome::MESSAGE_BOX_RESULT_NO;
      break;
  }

  if (result == chrome::MESSAGE_BOX_RESULT_NO) {
    disallowed_origins_[extension->id()].insert(origin);
    return false;
  }
  allowed_origins_[extension->id()].insert(origin);
  return true;
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
