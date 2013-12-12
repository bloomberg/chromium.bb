// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/suspicious_extension_bubble_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/suspicious_extension_bubble.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// UMA histogram constants.
enum UmaWipeoutHistogramOptions {
  ACTION_LEARN_MORE = 0,
  ACTION_DISMISS,
  ACTION_BOUNDARY, // Must be the last value.
};

static base::LazyInstance<extensions::ProfileKeyedAPIFactory<
    extensions::SuspiciousExtensionBubbleController> >
g_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// SuspiciousExtensionBubbleController

SuspiciousExtensionBubbleController::SuspiciousExtensionBubbleController(
    Profile* profile)
    : service_(extensions::ExtensionSystem::Get(profile)->extension_service()),
      profile_(profile),
      has_notified_(false) {
}

SuspiciousExtensionBubbleController::~SuspiciousExtensionBubbleController() {
}

// static
ProfileKeyedAPIFactory<SuspiciousExtensionBubbleController>*
SuspiciousExtensionBubbleController::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
SuspiciousExtensionBubbleController* SuspiciousExtensionBubbleController::Get(
    Profile* profile) {
  return ProfileKeyedAPIFactory<
      SuspiciousExtensionBubbleController>::GetForProfile(profile);
}

bool SuspiciousExtensionBubbleController::HasSuspiciousExtensions() {
  if (has_notified_)
    return false;
  if (!service_)
    return false;  // Can happen during tests.

  suspicious_extensions_.clear();

  ExtensionPrefs* prefs = service_->extension_prefs();

  scoped_ptr<const ExtensionSet> extension_set(
      service_->GenerateInstalledExtensionsSet());
  for (ExtensionSet::const_iterator it = extension_set->begin();
       it != extension_set->end(); ++it) {
    std::string id = (*it)->id();
    if (!prefs->IsExtensionDisabled(id))
      continue;
    int disble_reasons = prefs->GetDisableReasons(id);
    if (disble_reasons & Extension::DISABLE_NOT_VERIFIED) {
      if (prefs->HasWipeoutBeenAcknowledged(id))
         continue;

      suspicious_extensions_.push_back(id);
    }
  }

  UMA_HISTOGRAM_COUNTS_100(
      "SuspiciousExtensionBubble.ExtensionWipeoutCount",
      suspicious_extensions_.size());

  has_notified_ = true;
  return !suspicious_extensions_.empty();
}

void SuspiciousExtensionBubbleController::Show(
    SuspiciousExtensionBubble* bubble) {
  // Wire up all the callbacks, to get notified what actions the user took.
  base::Closure button_callback =
      base::Bind(&SuspiciousExtensionBubbleController::OnBubbleDismiss,
      base::Unretained(this));
  base::Closure link_callback =
      base::Bind(&SuspiciousExtensionBubbleController::OnLinkClicked,
      base::Unretained(this));
  bubble->OnButtonClicked(button_callback);
  bubble->OnLinkClicked(link_callback);

  content::RecordAction(
      content::UserMetricsAction("ExtensionWipeoutNotificationShown"));

  bubble->Show();
}

base::string16 SuspiciousExtensionBubbleController::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_SUSPICIOUS_DISABLED_TITLE);
}

base::string16 SuspiciousExtensionBubbleController::GetMessageBody() {
  return l10n_util::GetStringFUTF16(IDS_EXTENSIONS_SUSPICIOUS_DISABLED_BODY,
      l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE));
}

base::string16 SuspiciousExtensionBubbleController::GetOverflowText(
    const base::string16& overflow_count) {
  base::string16 overflow_string = l10n_util::GetStringUTF16(
      IDS_EXTENSIONS_SUSPICIOUS_DISABLED_AND_N_MORE);
  base::string16 new_string;

  // Just before string freeze, we checked in # as a substitution value for
  // this string, whereas we should have used $1. It was discovered too late,
  // so we do the substitution by hand in that case.
  if (overflow_string.find(ASCIIToUTF16("#")) != base::string16::npos) {
    base::ReplaceChars(overflow_string, ASCIIToUTF16("#").c_str(),
                       overflow_count, &new_string);
  } else {
    new_string = l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_SUSPICIOUS_DISABLED_AND_N_MORE,
            overflow_count);
  }

  return new_string;
}

base::string16 SuspiciousExtensionBubbleController::GetLearnMoreLabel() {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

base::string16 SuspiciousExtensionBubbleController::GetDismissButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_SUSPICIOUS_DISABLED_BUTTON);
}

std::vector<base::string16>
SuspiciousExtensionBubbleController::GetSuspiciousExtensionNames() {
  if (suspicious_extensions_.empty())
    return std::vector<base::string16>();

  std::vector<base::string16> return_value;
  for (ExtensionIdList::const_iterator it = suspicious_extensions_.begin();
       it != suspicious_extensions_.end(); ++it) {
    const Extension* extension = service_->GetInstalledExtension(*it);
    if (extension) {
      return_value.push_back(UTF8ToUTF16(extension->name()));
    } else {
      return_value.push_back(
          ASCIIToUTF16(std::string("(unknown name) ") + *it));
      // TODO(finnur): Add this as a string to the grd, for next milestone.
    }
  }
  return return_value;
}

void SuspiciousExtensionBubbleController::OnBubbleDismiss() {
  content::RecordAction(
      content::UserMetricsAction("SuspiciousExtensionBubbleDismissed"));
  UMA_HISTOGRAM_ENUMERATION("SuspiciousExtensionBubble.UserSelection",
                            ACTION_DISMISS, ACTION_BOUNDARY);
  AcknowledgeWipeout();
}

void SuspiciousExtensionBubbleController::OnLinkClicked() {
  UMA_HISTOGRAM_ENUMERATION("SuspiciousExtensionBubble.UserSelection",
                            ACTION_LEARN_MORE, ACTION_BOUNDARY);
  Browser* browser =
      chrome::FindBrowserWithProfile(profile_, chrome::GetActiveDesktop());
  if (browser) {
    browser->OpenURL(
        content::OpenURLParams(GURL(chrome::kChromeUIExtensionsURL),
                               content::Referrer(),
                               NEW_FOREGROUND_TAB,
                               content::PAGE_TRANSITION_LINK,
                               false));
  }
  AcknowledgeWipeout();
}

void SuspiciousExtensionBubbleController::AcknowledgeWipeout() {
  ExtensionPrefs* prefs = service_->extension_prefs();
  for (ExtensionIdList::const_iterator it = suspicious_extensions_.begin();
       it != suspicious_extensions_.end(); ++it) {
    prefs->SetWipeoutAcknowledged(*it, true);
  }
}

template <>
void ProfileKeyedAPIFactory<
    SuspiciousExtensionBubbleController>::DeclareFactoryDependencies() {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

}  // namespace extensions
