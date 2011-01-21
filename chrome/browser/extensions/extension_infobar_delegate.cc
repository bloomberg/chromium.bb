// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_infobar_delegate.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

ExtensionInfoBarDelegate::ExtensionInfoBarDelegate(Browser* browser,
                                                   TabContents* tab_contents,
                                                   const Extension* extension,
                                                   const GURL& url)
    : InfoBarDelegate(tab_contents),
      observer_(NULL),
      extension_(extension),
      tab_contents_(tab_contents),
      closing_(false) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  extension_host_.reset(manager->CreateInfobar(url, browser));
  extension_host_->set_associated_tab_contents(tab_contents);

  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 Source<Profile>(browser->profile()));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(browser->profile()));
}

ExtensionInfoBarDelegate::~ExtensionInfoBarDelegate() {
  if (observer_)
    observer_->OnDelegateDeleted();
}

bool ExtensionInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  ExtensionInfoBarDelegate* extension_delegate =
      delegate->AsExtensionInfoBarDelegate();
  // When an extension crashes, an InfoBar is shown (for the crashed extension).
  // That will result in a call to this function (to see if this InfoBarDelegate
  // is already showing the 'extension crashed InfoBar', which it never is), but
  // if it is our extension that crashes, the extension delegate is NULL so
  // we cannot check.
  if (!extension_delegate)
    return false;

  // Only allow one InfoBar at a time per extension.
  return extension_delegate->extension_host()->extension() ==
         extension_host_->extension();
}

void ExtensionInfoBarDelegate::InfoBarDismissed() {
  closing_ = true;
}

void ExtensionInfoBarDelegate::InfoBarClosed() {
  delete this;
}

InfoBarDelegate::Type ExtensionInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

ExtensionInfoBarDelegate*
    ExtensionInfoBarDelegate::AsExtensionInfoBarDelegate() {
  return this;
}

void ExtensionInfoBarDelegate::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
      const ExtensionHost* result = Details<ExtensionHost>(details).ptr();
      if (extension_host_.get() == result)
        tab_contents_->RemoveInfoBar(this);
      break;
    }
    case NotificationType::EXTENSION_UNLOADED: {
      const Extension* extension =
          Details<UnloadedExtensionInfo>(details)->extension;
      if (extension_ == extension)
        tab_contents_->RemoveInfoBar(this);
      break;
    }
    default: {
      NOTREACHED() << "Unknown message";
      break;
    }
  }
}
