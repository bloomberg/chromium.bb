// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_infobar_delegate.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"

ExtensionInfoBarDelegate::ExtensionInfoBarDelegate(
    Browser* browser,
    InfoBarTabHelper* infobar_helper,
    const Extension* extension,
    const GURL& url,
    int height)
        : InfoBarDelegate(infobar_helper),
          browser_(browser),
          observer_(NULL),
          extension_(extension),
          closing_(false) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  extension_host_.reset(manager->CreateInfobarHost(url, browser));
  extension_host_->set_associated_tab_contents(infobar_helper->tab_contents());

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 Source<Profile>(browser->profile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 Source<Profile>(browser->profile()));

#if defined(TOOLKIT_VIEWS) || defined(TOOLKIT_GTK)
  int default_height = InfoBar::kDefaultBarTargetHeight;
#elif defined(OS_MACOSX)
  // TODO(pkasting): Once Infobars have been ported to Mac, we can remove the
  // ifdefs and just use the Infobar constant below.
  int default_height = 36;
#endif
  height_ = std::max(0, height);
  height_ = std::min(2 * default_height, height_);
  if (height_ == 0)
    height_ = default_height;
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

InfoBarDelegate::Type ExtensionInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

ExtensionInfoBarDelegate*
    ExtensionInfoBarDelegate::AsExtensionInfoBarDelegate() {
  return this;
}

void ExtensionInfoBarDelegate::Observe(int type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE) {
    if (extension_host_.get() == Details<ExtensionHost>(details).ptr())
      RemoveSelf();
  } else {
    DCHECK(type == chrome::NOTIFICATION_EXTENSION_UNLOADED);
    if (extension_ == Details<UnloadedExtensionInfo>(details)->extension)
      RemoveSelf();
  }
}
