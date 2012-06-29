// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_warning_set.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_global_error_badge.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

// This class is used to represent warnings if extensions misbehave.
class ExtensionWarning {
 public:
  // Default constructor for storing ExtensionServiceWarning in STL containers
  // do not use.
  ExtensionWarning();

  // Constructs a warning of type |type| for extension |extension_id|. This
  // could be for example the fact that an extension conflicted with others.
  ExtensionWarning(ExtensionWarningSet::WarningType type,
                   const std::string& extension_id);

  ~ExtensionWarning();

  // Returns the specific warning type.
  ExtensionWarningSet::WarningType warning_type() const { return type_; }

  // Returns the id of the extension for which this warning is valid.
  const std::string& extension_id() const { return extension_id_; }

 private:
  ExtensionWarningSet::WarningType type_;
  std::string extension_id_;

  // Allow implicit copy and assign operator.
};

ExtensionWarning::ExtensionWarning() : type_(ExtensionWarningSet::kInvalid) {
}

ExtensionWarning::ExtensionWarning(
    ExtensionWarningSet::WarningType type,
    const std::string& extension_id)
    : type_(type), extension_id_(extension_id) {
  // These are invalid here because they do not have corresponding warning
  // messages in the UI.
  CHECK(type != ExtensionWarningSet::kInvalid);
  CHECK(type != ExtensionWarningSet::kMaxWarningType);
}

ExtensionWarning::~ExtensionWarning() {
}

bool operator<(const ExtensionWarning& a, const ExtensionWarning& b) {
  if (a.warning_type() == b.warning_type())
    return a.extension_id() < b.extension_id();
  return a.warning_type() < b.warning_type();
}

// Static
string16 ExtensionWarningSet::GetLocalizedWarning(
    ExtensionWarningSet::WarningType warning_type) {
  switch (warning_type) {
    case kInvalid:
    case kMaxWarningType:
      NOTREACHED();
      return string16();
    case kNetworkDelay:
      return l10n_util::GetStringFUTF16(
          IDS_EXTENSION_WARNINGS_NETWORK_DELAY,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
    case kNetworkConflict:
      return l10n_util::GetStringUTF16(IDS_EXTENSION_WARNINGS_NETWORK_CONFLICT);
    case kRepeatedCacheFlushes:
      return l10n_util::GetStringFUTF16(
          IDS_EXTENSION_WARNINGS_NETWORK_DELAY,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  }
  NOTREACHED();  // Switch statement has no default branch.
  return string16();
}

ExtensionWarningSet::ExtensionWarningSet(Profile* profile) : profile_(profile) {
}

ExtensionWarningSet::~ExtensionWarningSet() {
}

void ExtensionWarningSet::SetWarning(ExtensionWarningSet::WarningType type,
                                     const std::string& extension_id) {
  ExtensionWarning warning(type, extension_id);
  bool inserted = warnings_.insert(warning).second;
  if (inserted) {
    NotifyWarningsChanged();
    UpdateWarningBadge();
  }
}

void ExtensionWarningSet::ClearWarnings(
    const std::set<ExtensionWarningSet::WarningType>& types) {
  bool deleted_anything = false;
  for (iterator i = warnings_.begin(); i != warnings_.end();) {
    if (types.find(i->warning_type()) != types.end()) {
      deleted_anything = true;
      warnings_.erase(i++);
    } else {
      ++i;
    }
  }

  if (deleted_anything) {
    NotifyWarningsChanged();
    UpdateWarningBadge();
  }
}

void ExtensionWarningSet::GetWarningsAffectingExtension(
    const std::string& extension_id,
    std::set<ExtensionWarningSet::WarningType>* result) const {
  result->clear();
  for (const_iterator i = warnings_.begin(); i != warnings_.end(); ++i) {
    if (i->extension_id() == extension_id)
      result->insert(i->warning_type());
  }
}

// static
void ExtensionWarningSet::NotifyWarningsOnUI(
    void* profile_id,
    std::set<std::string> extension_ids,
    WarningType warning_type) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!profile ||
      !g_browser_process->profile_manager() ||
      !g_browser_process->profile_manager()->IsValidProfile(profile)) {
    return;
  }

  ExtensionWarningSet* warnings =
      profile->GetExtensionService()->extension_warnings();

  for (std::set<std::string>::const_iterator i = extension_ids.begin();
       i != extension_ids.end(); ++i) {
    warnings->SetWarning(warning_type, *i);
  }
}

void ExtensionWarningSet::SuppressBadgeForCurrentWarnings() {
  badge_suppressions_.insert(warnings_.begin(), warnings_.end());
  UpdateWarningBadge();
}

void ExtensionWarningSet::NotifyWarningsChanged() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_WARNING_CHANGED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
}

void ExtensionWarningSet::UpdateWarningBadge() {
  // We need a badge if a warning exists that has not been suppressed.
  bool need_warning_badge = false;
  for (const_iterator i = warnings_.begin(); i != warnings_.end(); ++i) {
    if (badge_suppressions_.find(*i) == badge_suppressions_.end()) {
      need_warning_badge = true;
      break;
    }
  }

  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(profile_);
  GlobalError* error = service->GetGlobalErrorByMenuItemCommandID(
      ExtensionGlobalErrorBadge::GetMenuItemCommandID());

  // Activate or hide the warning badge in case the current state is incorrect.
  if (error && !need_warning_badge) {
    service->RemoveGlobalError(error);
    delete error;
  } else if (!error && need_warning_badge) {
    service->AddGlobalError(new ExtensionGlobalErrorBadge);
  }
}
