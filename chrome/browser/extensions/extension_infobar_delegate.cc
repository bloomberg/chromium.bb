// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_infobar_delegate.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

ExtensionInfoBarDelegate::ExtensionInfoBarDelegate(Browser* browser,
                                                   TabContents* tab_contents,
                                                   Extension* extension,
                                                   const GURL& url)
    : InfoBarDelegate(tab_contents),
      extension_(extension),
      tab_contents_(tab_contents) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  extension_host_.reset(manager->CreateInfobar(url, browser));

  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 Source<Profile>(browser->profile()));
}

ExtensionInfoBarDelegate::~ExtensionInfoBarDelegate() {
}

bool ExtensionInfoBarDelegate::EqualsDelegate(InfoBarDelegate* delegate) const {
  ExtensionInfoBarDelegate* extension_delegate =
      delegate->AsExtensionInfoBarDelegate();
  // Only allow one InfoBar at a time per extension.
  return extension_delegate->extension_host()->extension() ==
         extension_host_->extension();
}

void ExtensionInfoBarDelegate::InfoBarClosed() {
  delete this;
}

#if !defined(TOOLKIT_VIEWS)
InfoBar* ExtensionInfoBarDelegate::CreateInfoBar() {
  NOTIMPLEMENTED();
  return NULL;
}
#endif  // !TOOLKIT_VIEWS

void ExtensionInfoBarDelegate::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  DCHECK(type == NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE);
  const ExtensionHost* result = Details<ExtensionHost>(details).ptr();
  if (extension_host_.get() == result)
    tab_contents_->RemoveInfoBar(this);
}
